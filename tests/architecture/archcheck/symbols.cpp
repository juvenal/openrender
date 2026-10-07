/**
 * Project: openRender
 *
 * File: symbols.cpp
 *
 * Description:
 *   nm scanning and link-edge attribution (see symbols.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "symbols.h"

#include <cstdlib>
#include <cxxabi.h>
#include <sstream>
#include <unordered_map>

#include "process.h"

namespace archcheck {

    namespace {

        constexpr std::size_t kBatch = 64;

        // Undefined-symbol types: U (both nms), w / v (weak undefined, GNU).
        bool isUndefined(std::string_view type) {
            return type == "U" || type == "w" || type == "v";
        }

    } // namespace

    SymbolTable scanSymbols(const std::string &nm, const std::vector<std::pair<std::string, std::string>> &objectSources, bool stripUnderscore) {
        SymbolTable t;
        std::unordered_map<std::string, std::string> sourceOf(objectSources.begin(), objectSources.end());

        for (std::size_t start = 0; start < objectSources.size(); start += kBatch) {
            std::vector<std::string> argv = {nm, "-g", "-A"};
            for (std::size_t i = start; i < objectSources.size() && i < start + kBatch; i++)
                argv.push_back(objectSources[i].first);
            const ProcessResult r = runProcess(argv);
            if (r.status == 127) {
                t.error = "'" + nm + "' not found (needed for link-level edges)";
                return t;
            }
            if (r.status != 0) {
                t.error = "'" + nm + "' failed with status " + std::to_string(r.status);
                return t;
            }

            // "<object>: <value> <type> <name>" (Apple) or "<object>:<value> <type>
            // <name>" (GNU); undefined symbols have no value. Object paths carry
            // no ':' of their own.
            std::istringstream in(r.output);
            std::string line;
            while (std::getline(in, line)) {
                const std::size_t colon = line.find(':');
                if (colon == std::string::npos)
                    continue;
                const auto src = sourceOf.find(line.substr(0, colon));
                if (src == sourceOf.end())
                    continue;
                std::istringstream fields(line.substr(colon + 1));
                std::vector<std::string> f;
                for (std::string w; fields >> w;)
                    f.push_back(w);
                if (f.size() < 2)
                    continue;
                const std::string &type = f[f.size() - 2];
                std::string name = f.back();
                if (stripUnderscore && !name.empty() && name[0] == '_')
                    name.erase(0, 1);
                if (isUndefined(type))
                    t.references[src->second].insert(name);
                else
                    t.definers[name].insert(src->second);
            }
        }

        // A source compiled into several objects may define in one what another
        // copy references; a file never depends on itself.
        for (auto &[file, refs] : t.references)
            for (auto it = refs.begin(); it != refs.end();) {
                const auto d = t.definers.find(*it);
                it = d != t.definers.end() && d->second.count(file) ? refs.erase(it) : std::next(it);
            }
        return t;
    }

    std::vector<LinkEdge> attributeLinkEdges(const SymbolTable &table, const ClassOf &classOf) {
        std::vector<LinkEdge> edges;
        for (const auto &[file, refs] : table.references) {
            const auto fromClass = classOf(file);
            if (!fromClass)
                continue;
            for (const std::string &sym : refs) {
                const auto d = table.definers.find(sym);
                if (d == table.definers.end())
                    continue; // system or third-party symbol
                LinkEdge e;
                std::set<Class> classes;
                bool benign = false;
                for (const std::string &definer : d->second) {
                    const auto c = classOf(definer);
                    if (!c)
                        continue;
                    if (*c == *fromClass || *c == Class::Infra) {
                        benign = true;
                        break;
                    }
                    classes.insert(*c);
                    e.definedIn.push_back(definer);
                }
                if (benign || classes.empty())
                    continue;
                e.from = file;
                e.symbol = sym;
                e.toClasses.assign(classes.begin(), classes.end());
                edges.push_back(std::move(e));
            }
        }
        return edges;
    }

    std::string demangle(const std::string &symbol) {
        if (symbol.rfind("_Z", 0) != 0)
            return symbol;
        int status = 0;
        char *out = abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
        if (status != 0 || out == nullptr)
            return symbol;
        std::string s(out);
        std::free(out);
        return s;
    }

} // namespace archcheck
