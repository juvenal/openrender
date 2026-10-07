/**
 * Project: openRender
 *
 * File: graph.cpp
 *
 * Description:
 *   Edge classification and domain-cycle detection (see graph.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "graph.h"

#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <set>
#include <tuple>

namespace archcheck {

    namespace {

        std::size_t idx(Class c) {
            return static_cast<std::size_t>(c);
        }

        // What crosses one class-to-class hop.
        struct Hop {
                bool present = false;
                bool violation = false;
                bool allowlisted = false;
                const Edge *example = nullptr; // a violation if any, else allowlisted, else allowed

                void add(const Edge &e) {
                    present = true;
                    violation = violation || e.status == EdgeStatus::Violation;
                    allowlisted = allowlisted || e.status == EdgeStatus::Allowlisted;
                    auto rank = [](const Edge *x) { return x->status == EdgeStatus::Violation ? 0 : x->status == EdgeStatus::Allowlisted ? 1
                                                                                                                                         : 2; };
                    if (example == nullptr ||
                        std::make_tuple(rank(&e), e.from, e.to) < std::make_tuple(rank(example), example->from, example->to))
                        example = &e;
                }
        };

        using HopMatrix = std::array<std::array<Hop, kClassCount>, kClassCount>;

        // Tarjan's strongly connected components over the class graph.
        std::array<int, kClassCount> components(const HopMatrix &hops) {
            std::array<int, kClassCount> comp, index, low;
            comp.fill(-1);
            index.fill(-1);
            low.fill(0);
            std::array<bool, kClassCount> onStack{};
            std::vector<std::size_t> stack;
            int counter = 0, ncomp = 0;

            auto strong = [&](auto &&self, std::size_t v) -> void {
                index[v] = low[v] = counter++;
                stack.push_back(v);
                onStack[v] = true;
                for (std::size_t w = 0; w < kClassCount; w++) {
                    if (w == v || !hops[v][w].present)
                        continue;
                    if (index[w] < 0) {
                        self(self, w);
                        low[v] = std::min(low[v], low[w]);
                    }
                    else if (onStack[w]) {
                        low[v] = std::min(low[v], index[w]);
                    }
                }
                if (low[v] == index[v]) {
                    std::size_t w;
                    do {
                        w = stack.back();
                        stack.pop_back();
                        onStack[w] = false;
                        comp[w] = ncomp;
                    } while (w != v);
                    ncomp++;
                }
            };
            for (std::size_t v = 0; v < kClassCount; v++)
                if (index[v] < 0)
                    strong(strong, v);
            return comp;
        }

        // Shortest path from `from` to `to` inside one component (BFS), inclusive.
        std::vector<std::size_t> shortestPath(const HopMatrix &hops, const std::array<int, kClassCount> &comp, std::size_t from, std::size_t to) {
            std::array<int, kClassCount> prev;
            prev.fill(-2);
            prev[from] = -1;
            std::deque<std::size_t> queue{from};
            while (!queue.empty()) {
                const std::size_t v = queue.front();
                queue.pop_front();
                if (v == to)
                    break;
                for (std::size_t w = 0; w < kClassCount; w++) {
                    if (w == v || !hops[v][w].present || comp[w] != comp[from] || prev[w] != -2)
                        continue;
                    prev[w] = static_cast<int>(v);
                    queue.push_back(w);
                }
            }
            std::vector<std::size_t> path;
            if (prev[to] == -2)
                return path;
            for (int v = static_cast<int>(to); v >= 0; v = prev[v])
                path.push_back(static_cast<std::size_t>(v));
            std::reverse(path.begin(), path.end());
            return path;
        }

        std::string edgeLabel(const Edge &e) {
            return e.from + " -> " + e.to;
        }

    } // namespace

    void classifyEdges(std::vector<Edge> &edges, const RuleTable &rules, const std::vector<AllowlistEntry> &allowlist, Platform platform, std::vector<bool> &matched) {
        std::map<std::tuple<EdgeKind, std::string, std::string>, std::size_t> applying;
        for (std::size_t i = 0; i < allowlist.size(); i++) {
            const AllowlistEntry &a = allowlist[i];
            if (!a.platform || *a.platform == platform)
                applying.emplace(std::make_tuple(a.kind, a.fromFile, a.to), i);
        }
        matched.assign(allowlist.size(), false);

        for (Edge &e : edges) {
            const bool allowed = std::all_of(e.toClasses.begin(), e.toClasses.end(),
                                             [&](Class to) { return rules.allows(e.fromClass, to); });
            if (allowed) {
                e.status = EdgeStatus::Allowed;
                continue;
            }
            if (const auto it = applying.find(std::make_tuple(e.kind, e.from, e.to)); it != applying.end()) {
                e.status = EdgeStatus::Allowlisted;
                e.registryId = allowlist[it->second].registryId;
                matched[it->second] = true;
                continue;
            }
            e.status = EdgeStatus::Violation;
        }
    }

    AllowlistCheck checkAllowlist(const std::vector<AllowlistEntry> &allowlist, const std::vector<bool> &matched, const std::vector<Edge> &edges, const std::map<std::string, RegistryEntry> &registry, Platform platform, const std::set<std::string> &includeCompiled, const std::set<std::string> &linkCompiled) {
        std::map<std::tuple<EdgeKind, std::string, std::string>, EdgeStatus> present;
        for (const Edge &e : edges)
            present.emplace(std::make_tuple(e.kind, e.from, e.to), e.status);

        AllowlistCheck out;
        for (std::size_t i = 0; i < allowlist.size(); i++) {
            const AllowlistEntry &a = allowlist[i];
            // An entry tagged for the other platform is neither matched nor checked.
            if (a.platform && *a.platform != platform)
                continue;
            auto issue = [&](std::string reason) { return EntryIssue{a.registryId, a.kind, a.fromFile, a.to, std::move(reason), a.line}; };

            const auto reg = registry.find(a.registryId);
            if (reg == registry.end())
                out.badIds.push_back(issue("unknown"));
            else if (reg->second.status == RegistryStatus::Resolved)
                out.badIds.push_back(issue("resolved"));

            if (matched[i])
                continue;
            const auto &compiled = a.kind == EdgeKind::Include ? includeCompiled : linkCompiled;
            if (!compiled.count(a.fromFile)) {
                out.notBuilt.push_back(issue(""));
                continue;
            }
            const auto p = present.find(std::make_tuple(a.kind, a.fromFile, a.to));
            out.stale.push_back(issue(p != present.end() && p->second == EdgeStatus::Allowed ? "now allowed"
                                                                                             : "no longer present"));
        }
        return out;
    }

    std::string ruleText(const RuleTable &rules, Class from) {
        std::string allowed;
        for (std::size_t i = 0; i < kClassCount; i++) {
            const Class to = static_cast<Class>(i);
            if (to == from || !rules.allows(from, to))
                continue;
            if (!allowed.empty())
                allowed += ", ";
            allowed += className(to);
        }
        return "allowed-dependency table (" + std::string(className(from)) +
               " may depend on: " + (allowed.empty() ? "nothing" : allowed) + ")";
    }

    std::vector<Cycle> findCycles(const std::vector<Edge> &edges) {
        HopMatrix hops{};
        for (const Edge &e : edges)
            for (const Class to : e.toClasses)
                if (to != e.fromClass)
                    hops[idx(e.fromClass)][idx(to)].add(e);
        const auto comp = components(hops);

        std::map<std::string, Cycle> found; // keyed by path text, for dedup and order
        for (std::size_t a = 0; a < kClassCount; a++) {
            for (std::size_t b = 0; b < kClassCount; b++) {
                const Hop &h = hops[a][b];
                // The allowed graph is acyclic, so every cycle crosses a hop that
                // is not purely allowed; start from each such hop.
                if (a == b || !h.present || comp[a] != comp[b] || (!h.violation && !h.allowlisted))
                    continue;
                std::vector<std::size_t> path = {a};
                const auto back = shortestPath(hops, comp, b, a);
                if (back.empty())
                    continue;
                path.insert(path.end(), back.begin(), back.end()); // a, b, ..., a

                // Rotate so the alphabetically first class leads.
                path.pop_back();
                const auto first = std::min_element(path.begin(), path.end(), [](std::size_t x, std::size_t y) {
                    return className(static_cast<Class>(x)) < className(static_cast<Class>(y));
                });
                std::rotate(path.begin(), first, path.end());
                path.push_back(path.front());

                Cycle c;
                std::string key;
                for (std::size_t i = 0; i < path.size(); i++) {
                    c.path.push_back(static_cast<Class>(path[i]));
                    key += std::string(className(static_cast<Class>(path[i]))) + ">";
                    if (i + 1 < path.size()) {
                        const Hop &hop = hops[path[i]][path[i + 1]];
                        c.edges.push_back(edgeLabel(*hop.example));
                        c.failing = c.failing || hop.violation;
                    }
                }
                found.emplace(key, std::move(c));
            }
        }
        std::vector<Cycle> out;
        for (auto &[key, c] : found)
            out.push_back(std::move(c));
        return out;
    }

} // namespace archcheck
