/**
 * Project: openRender
 *
 * File: includes.cpp
 *
 * Description:
 *   Depfile parsing and #include extraction (see includes.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "includes.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>

#include "process.h"

namespace archcheck {

    namespace {

        bool isSpace(char c) {
            return c == ' ' || c == '\t';
        }

        namespace fs = std::filesystem;

        bool readFile(const fs::path &p, std::string &out) {
            std::ifstream f(p, std::ios::binary);
            if (!f)
                return false;
            std::ostringstream ss;
            ss << f.rdbuf();
            out = ss.str();
            return true;
        }

        bool hasExtension(std::string_view path, std::initializer_list<std::string_view> exts) {
            const std::size_t dot = path.rfind('.');
            if (dot == std::string_view::npos || path.find('/', dot) != std::string_view::npos)
                return false;
            for (const std::string_view e : exts)
                if (path.substr(dot) == e)
                    return true;
            return false;
        }

        bool isCompiledSource(std::string_view path) {
            return hasExtension(path, {".c", ".cc", ".cpp"});
        }

        // Whitespace-separated words of a compile command or flags line; double and
        // single quotes group, a backslash escapes the next character.
        std::vector<std::string> shellWords(std::string_view s) {
            std::vector<std::string> out;
            std::string cur;
            bool inWord = false;
            char quote = 0;
            for (std::size_t i = 0; i < s.size(); i++) {
                const char c = s[i];
                if (quote != 0) {
                    if (c == quote)
                        quote = 0;
                    else
                        cur += c;
                }
                else if (c == '"' || c == '\'') {
                    quote = c;
                    inWord = true;
                }
                else if (c == '\\' && i + 1 < s.size()) {
                    cur += s[++i];
                    inWord = true;
                }
                else if (std::isspace(static_cast<unsigned char>(c))) {
                    if (inWord)
                        out.push_back(std::move(cur));
                    cur.clear();
                    inWord = false;
                }
                else {
                    cur += c;
                    inWord = true;
                }
            }
            if (inWord)
                out.push_back(std::move(cur));
            return out;
        }

        // The header search order encoded by -iquote / -I / -isystem flags.
        SearchPath searchPathFromFlags(const std::vector<std::string> &words) {
            std::vector<std::string> iquote, inc, isystem;
            for (std::size_t i = 0; i < words.size(); i++) {
                const std::string &w = words[i];
                auto take = [&](std::string_view flag, std::vector<std::string> &dst) {
                    if (w == flag) {
                        if (i + 1 < words.size())
                            dst.push_back(words[++i]);
                        return true;
                    }
                    if (w.size() > flag.size() && w.compare(0, flag.size(), flag) == 0) {
                        dst.push_back(w.substr(flag.size()));
                        return true;
                    }
                    return false;
                };
                // -isystem and -iquote first: "-I" is a prefix of neither, but keep
                // the longer flags ahead of the shorter one regardless.
                if (take("-isystem", isystem) || take("-iquote", iquote) || take("-I", inc))
                    continue;
            }
            SearchPath sp;
            sp.quoted = iquote;
            sp.quoted.insert(sp.quoted.end(), inc.begin(), inc.end());
            sp.quoted.insert(sp.quoted.end(), isystem.begin(), isystem.end());
            sp.angled = inc;
            sp.angled.insert(sp.angled.end(), isystem.begin(), isystem.end());
            return sp;
        }

        // CMakeCache.txt entries this tool needs.
        std::map<std::string, std::string> readCache(const std::string &text) {
            std::map<std::string, std::string> out;
            std::istringstream in(text);
            std::string line;
            while (std::getline(in, line)) {
                const std::size_t colon = line.find(':');
                const std::size_t eq = line.find('=');
                if (colon == std::string::npos || eq == std::string::npos || colon > eq || line[0] == '#' ||
                    line[0] == '/')
                    continue;
                out[line.substr(0, colon)] = line.substr(eq + 1);
            }
            return out;
        }

        // Interns search paths so translation units of one target share an index.
        struct SearchPathTable {
                std::vector<SearchPath> paths;
                std::map<std::pair<std::vector<std::string>, std::vector<std::string>>, std::size_t> index;

                std::size_t intern(SearchPath sp) {
                    auto key = std::make_pair(sp.quoted, sp.angled);
                    if (const auto it = index.find(key); it != index.end())
                        return it->second;
                    paths.push_back(std::move(sp));
                    index.emplace(std::move(key), paths.size() - 1);
                    return paths.size() - 1;
                }
        };

        // Builds one translation unit from its depfile deps; false on parse error.
        bool addUnit(BuildTree &tree, const PathMapper &mapper, const std::string &source, const std::string &object, const std::vector<std::string> &deps, std::size_t searchPath) {
            TranslationUnit u;
            u.source = source;
            u.object = object;
            u.searchPath = searchPath;
            u.deps.insert(source);
            for (const std::string &d : deps)
                if (const auto m = mapper.map(d))
                    u.deps.insert(*m);
            tree.units.push_back(std::move(u));
            return true;
        }

        std::string incompleteMessage(std::size_t missing, std::size_t total, const std::string &example) {
            return "incomplete build tree: " + std::to_string(missing) + " of " + std::to_string(total) +
                   " in-scope objects or depfiles are missing (e.g. " + example + "); build the tree first";
        }

        void loadMakefiles(BuildTree &tree, const PathMapper &mapper, const ScopeFn &inScope) {
            const fs::path build(mapper.buildDir());
            SearchPathTable table;
            std::map<std::string, std::size_t> flagsCache; // flags.make + language -> index
            std::size_t total = 0, missing = 0;
            std::string example;

            std::vector<fs::path> infos;
            std::error_code ec;
            for (auto it = fs::recursive_directory_iterator(build, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (ec)
                    break;
                if (it->path().filename() == "DependInfo.cmake")
                    infos.push_back(it->path());
            }
            std::sort(infos.begin(), infos.end());

            for (const fs::path &info : infos) {
                std::string text;
                if (!readFile(info, text))
                    continue;
                const std::size_t start = text.find("set(CMAKE_DEPENDS_DEPENDENCY_FILES");
                if (start == std::string::npos)
                    continue;
                const std::size_t end = text.find(')', start);
                // Quoted strings come in groups of four: source, object, kind, depfile.
                std::vector<std::string> q;
                for (std::size_t i = text.find('"', start); i != std::string::npos && i < end;) {
                    const std::size_t close = text.find('"', i + 1);
                    if (close == std::string::npos)
                        break;
                    q.push_back(text.substr(i + 1, close - i - 1));
                    i = text.find('"', close + 1);
                }
                for (std::size_t g = 0; g + 3 < q.size(); g += 4) {
                    const std::string &src = q[g];
                    if (!isCompiledSource(src))
                        continue;
                    const auto mapped = mapper.map(src);
                    if (!mapped || !inScope(*mapped))
                        continue;
                    total++;
                    const fs::path obj = build / q[g + 1];
                    const fs::path dep = build / q[g + 3];
                    std::string depText;
                    if (!fs::exists(obj) || !readFile(dep, depText)) {
                        if (missing++ == 0)
                            example = q[g + 1] + " for " + *mapped;
                        continue;
                    }
                    const auto parsed = parseDepfile(depText, dep.generic_string());
                    if (!parsed.ok()) {
                        tree.error = parsed.errors.front().str();
                        return;
                    }
                    std::vector<std::string> deps;
                    for (const DepfileRule &r : *parsed.value)
                        deps.insert(deps.end(), r.deps.begin(), r.deps.end());

                    const std::string lang = hasExtension(src, {".c"}) ? "C" : "CXX";
                    const std::string flagsKey = info.parent_path().generic_string() + "|" + lang;
                    auto fc = flagsCache.find(flagsKey);
                    if (fc == flagsCache.end()) {
                        std::string flags;
                        readFile(info.parent_path() / "flags.make", flags);
                        std::vector<std::string> words;
                        std::istringstream in(flags);
                        std::string line;
                        const std::string prefix = lang + "_INCLUDES = ";
                        while (std::getline(in, line))
                            if (line.rfind(prefix, 0) == 0)
                                words = shellWords(std::string_view(line).substr(prefix.size()));
                        fc = flagsCache.emplace(flagsKey, table.intern(searchPathFromFlags(words))).first;
                    }
                    addUnit(tree, mapper, *mapped, obj.generic_string(), deps, fc->second);
                }
            }
            tree.searchPaths = std::move(table.paths);
            if (missing > 0)
                tree.error = incompleteMessage(missing, total, example);
        }

        // Ninja keeps no depfiles: deps come from `ninja -t deps`, compile commands
        // (sources, objects, include flags) from `ninja -t commands`.
        void loadNinja(BuildTree &tree, const PathMapper &mapper, const ScopeFn &inScope) {
            const std::string build = mapper.buildDir();
            const ProcessResult cmds = runProcess({"ninja", "-C", build, "-t", "commands"});
            if (cmds.status == 127) {
                tree.error = "ninja not found (needed to read a Ninja build tree)";
                return;
            }
            if (cmds.status != 0) {
                tree.error = "`ninja -t commands` failed with status " + std::to_string(cmds.status);
                return;
            }
            const ProcessResult depsOut = runProcess({"ninja", "-C", build, "-t", "deps"});
            if (depsOut.status != 0) {
                tree.error = "`ninja -t deps` failed with status " + std::to_string(depsOut.status);
                return;
            }

            // object (as ninja names it) -> deps
            std::map<std::string, std::vector<std::string>> deps;
            {
                std::istringstream in(depsOut.output);
                std::string line, current;
                while (std::getline(in, line)) {
                    if (line.empty()) {
                        current.clear();
                    }
                    else if (line[0] == ' ' || line[0] == '\t') {
                        if (!current.empty()) {
                            std::size_t b = line.find_first_not_of(" \t");
                            deps[current].push_back(line.substr(b));
                        }
                    }
                    else if (const std::size_t colon = line.find(": #deps"); colon != std::string::npos) {
                        current = line.substr(0, colon);
                        deps[current];
                    }
                }
            }

            SearchPathTable table;
            std::size_t total = 0, missing = 0;
            std::string example;
            std::istringstream in(cmds.output);
            std::string line;
            while (std::getline(in, line)) {
                const auto words = shellWords(line);
                std::string src, obj;
                for (std::size_t i = 0; i + 1 < words.size(); i++) {
                    if (words[i] == "-c")
                        src = words[i + 1];
                    else if (words[i] == "-o")
                        obj = words[i + 1];
                }
                if (src.empty() || obj.empty() || !isCompiledSource(src))
                    continue;
                const fs::path srcPath = fs::path(src).is_absolute() ? fs::path(src) : fs::path(build) / src;
                const auto mapped = mapper.map(srcPath.generic_string());
                if (!mapped || !inScope(*mapped))
                    continue;
                total++;
                const fs::path objPath = fs::path(obj).is_absolute() ? fs::path(obj) : fs::path(build) / obj;
                const auto d = deps.find(obj);
                if (d == deps.end() || !fs::exists(objPath)) {
                    if (missing++ == 0)
                        example = obj + " for " + *mapped;
                    continue;
                }
                addUnit(tree, mapper, *mapped, objPath.generic_string(), d->second,
                        table.intern(searchPathFromFlags(words)));
            }
            tree.searchPaths = std::move(table.paths);
            if (missing > 0)
                tree.error = incompleteMessage(missing, total, example);
        }

        // Splits one logical depfile line into words, undoing make's escapes
        // ("\ " -> ' ', "\#" -> '#', "$$" -> '$').
        std::vector<std::string> depWords(std::string_view s) {
            std::vector<std::string> out;
            std::string cur;
            bool inWord = false;
            for (std::size_t i = 0; i < s.size(); i++) {
                const char c = s[i];
                if (c == '\\' && i + 1 < s.size() && (s[i + 1] == ' ' || s[i + 1] == '#')) {
                    cur += s[++i];
                    inWord = true;
                }
                else if (c == '$' && i + 1 < s.size() && s[i + 1] == '$') {
                    cur += '$';
                    i++;
                    inWord = true;
                }
                else if (isSpace(c)) {
                    if (inWord)
                        out.push_back(std::move(cur));
                    cur.clear();
                    inWord = false;
                }
                else {
                    cur += c;
                    inWord = true;
                }
            }
            if (inWord)
                out.push_back(std::move(cur));
            return out;
        }

        // Index of the rule separator: the first unescaped ':' followed by
        // whitespace or the end of the line.
        std::size_t ruleColon(std::string_view s) {
            for (std::size_t i = 0; i < s.size(); i++) {
                if (s[i] == '\\') {
                    i++;
                    continue;
                }
                if (s[i] == ':' && (i + 1 == s.size() || isSpace(s[i + 1])))
                    return i;
            }
            return std::string_view::npos;
        }

    } // namespace

    Parsed<std::vector<DepfileRule>> parseDepfile(std::string_view text, std::string_view label) {
        Parsed<std::vector<DepfileRule>> r;
        std::vector<DepfileRule> rules;

        // Join backslash-newline continuations into logical lines, remembering
        // the physical line each one starts on.
        std::string logical;
        int startLine = 0;
        int lineNo = 0;
        auto flush = [&]() {
            std::size_t b = 0;
            while (b < logical.size() && isSpace(logical[b]))
                b++;
            if (b < logical.size()) {
                const std::string_view s(logical);
                const std::size_t colon = ruleColon(s);
                if (colon == std::string_view::npos) {
                    r.errors.push_back({std::string(label), startLine, "expected ':' after the rule targets"});
                }
                else {
                    DepfileRule rule;
                    rule.targets = depWords(s.substr(0, colon));
                    rule.deps = depWords(s.substr(colon + 1));
                    rules.push_back(std::move(rule));
                }
            }
            logical.clear();
            startLine = 0;
        };

        std::size_t pos = 0;
        while (pos < text.size()) {
            lineNo++;
            const std::size_t nl = text.find('\n', pos);
            std::string_view line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
            pos = nl == std::string_view::npos ? text.size() : nl + 1;
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            if (startLine == 0)
                startLine = lineNo;
            // An odd number of trailing backslashes continues the line.
            std::size_t slashes = 0;
            while (slashes < line.size() && line[line.size() - 1 - slashes] == '\\')
                slashes++;
            if (slashes % 2 == 1) {
                logical.append(line.substr(0, line.size() - 1));
                logical += ' ';
                continue;
            }
            logical.append(line);
            flush();
        }
        flush();

        if (r.errors.empty())
            r.value = std::move(rules);
        return r;
    }

    std::vector<IncludeDirective> extractIncludes(std::string_view source) {
        std::vector<IncludeDirective> out;
        bool inBlockComment = false;
        int lineNo = 0;
        std::size_t pos = 0;

        while (pos < source.size()) {
            lineNo++;
            const std::size_t nl = source.find('\n', pos);
            const std::string_view line =
                source.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
            pos = nl == std::string_view::npos ? source.size() : nl + 1;

            // The line with comments blanked out. String and character literals
            // are copied through so comment markers inside them are not taken
            // for comments.
            std::string code;
            code.reserve(line.size());
            for (std::size_t i = 0; i < line.size(); i++) {
                const char c = line[i];
                if (inBlockComment) {
                    if (c == '*' && i + 1 < line.size() && line[i + 1] == '/') {
                        inBlockComment = false;
                        i++;
                    }
                    code += ' ';
                }
                else if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
                    break;
                }
                else if (c == '/' && i + 1 < line.size() && line[i + 1] == '*') {
                    inBlockComment = true;
                    code += ' ';
                    i++;
                }
                else if (c == '"' || c == '\'') {
                    code += c;
                    for (i++; i < line.size() && line[i] != c; i++) {
                        if (line[i] == '\\' && i + 1 < line.size())
                            code += line[i++];
                        code += line[i];
                    }
                    if (i < line.size())
                        code += c;
                }
                else {
                    code += c;
                }
            }

            std::size_t i = 0;
            auto skipSpace = [&]() {
                while (i < code.size() && std::isspace(static_cast<unsigned char>(code[i])))
                    i++;
            };
            skipSpace();
            if (i >= code.size() || code[i] != '#')
                continue;
            i++;
            skipSpace();
            const std::size_t kw = i;
            while (i < code.size() && (std::isalnum(static_cast<unsigned char>(code[i])) || code[i] == '_'))
                i++;
            if (std::string_view(code).substr(kw, i - kw) != "include")
                continue;
            skipSpace();
            if (i >= code.size() || (code[i] != '"' && code[i] != '<'))
                continue; // computed include (#include MACRO)
            const char close = code[i] == '"' ? '"' : '>';
            const std::size_t end = code.find(close, i + 1);
            if (end == std::string::npos)
                continue;
            out.push_back({code.substr(i + 1, end - i - 1), close == '>', lineNo});
        }
        return out;
    }

    BuildTree loadBuildTree(const PathMapper &mapper, const ScopeFn &inScope) {
        BuildTree tree;
        const fs::path build(mapper.buildDir());
        std::string cacheText;
        if (!readFile(build / "CMakeCache.txt", cacheText)) {
            tree.error = mapper.buildDir() + " is not a CMake build tree (no CMakeCache.txt)";
            return tree;
        }
        const auto cache = readCache(cacheText);
        if (const auto it = cache.find("CMAKE_GENERATOR"); it != cache.end())
            tree.generator = it->second;
        const auto nm = cache.find("CMAKE_NM");
        tree.nm = nm != cache.end() && !nm->second.empty() ? nm->second : "nm";

        if (tree.generator == "Unix Makefiles")
            loadMakefiles(tree, mapper, inScope);
        else if (tree.generator == "Ninja")
            loadNinja(tree, mapper, inScope);
        else
            tree.error = "unsupported generator '" + tree.generator + "' (Unix Makefiles or Ninja)";

        if (tree.error.empty() && tree.units.empty())
            tree.error = "no compiled translation units for " + mapper.sourceDir() + "src/ in " + mapper.buildDir() +
                         " (configure and build first)";
        return tree;
    }

    IncludeResolution resolveIncludes(const BuildTree &tree, const PathMapper &mapper, const ScopeFn &inScope) {
        IncludeResolution res;
        std::unordered_map<std::string, std::vector<IncludeDirective>> directives;
        // Search results: the first existing candidate, mapped (nullopt when it
        // lies outside both trees); a missing key component = not found at all.
        struct Found {
                bool found = false;
                std::optional<std::string> mapped;
        };
        std::unordered_map<std::string, Found> memo;

        auto absolute = [&](const std::string &mapped) {
            if (mapped.rfind("@build/", 0) == 0)
                return mapper.buildDir() + mapped.substr(7);
            return mapper.sourceDir() + mapped;
        };
        auto search = [&](const SearchPath &sp, std::size_t spIndex, const IncludeDirective &d,
                          const std::string &includerDir) -> const Found & {
            const std::string key = std::to_string(spIndex) + (d.angled ? "<" : "\"") +
                                    (d.angled ? std::string() : includerDir) + "|" + d.name;
            if (const auto it = memo.find(key); it != memo.end())
                return it->second;
            Found f;
            auto tryDir = [&](const std::string &dir) {
                const fs::path p = fs::path(dir) / d.name;
                std::error_code ec;
                if (!fs::is_regular_file(p, ec))
                    return false;
                f.found = true;
                f.mapped = mapper.map(p.generic_string());
                return true;
            };
            bool done = false;
            if (!d.angled)
                done = tryDir(includerDir);
            for (const std::string &dir : d.angled ? sp.angled : sp.quoted) {
                if (done)
                    break;
                done = tryDir(dir);
            }
            return memo.emplace(key, f).first->second;
        };

        for (const TranslationUnit &u : tree.units) {
            const SearchPath &sp = tree.searchPaths[u.searchPath];
            for (const std::string &file : u.deps) {
                if (!inScope(file))
                    continue;
                res.seen.insert(file);
                auto dit = directives.find(file);
                if (dit == directives.end()) {
                    std::string text;
                    readFile(absolute(file), text);
                    dit = directives.emplace(file, extractIncludes(text)).first;
                }
                const std::string includerDir = fs::path(absolute(file)).parent_path().generic_string();
                for (const IncludeDirective &d : dit->second) {
                    const Found &f = search(sp, u.searchPath, d, includerDir);
                    if (!f.found || !f.mapped || *f.mapped == file)
                        continue;
                    if (!u.deps.count(*f.mapped) || !inScope(*f.mapped))
                        continue;
                    res.edges.emplace(file, *f.mapped);
                }
            }
        }
        return res;
    }

} // namespace archcheck
