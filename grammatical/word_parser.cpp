#include "phrase.h"
#include "ranged.h"
#include "tokens.h"
#include "parser.h"

#include <cassert>
#include <cctype>
#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>


using std::pair;
using std::shared_ptr;
using std::vector;
using std::string;
using std::string_view;



static bool ignore_case_less(string_view a, string_view b)
{
	auto ita = a.begin(); const auto enda = a.end();
	auto itb = b.begin(); const auto endb = b.end();
	for (; ita != enda; ++ita, ++itb)
	{
		if (itb == endb) return false;
		const auto ca = std::tolower(*ita);
		const auto cb = std::tolower(*itb);
		if (cb < ca) return false;
		if (ca < cb) return true;
	}
	return itb != endb;
}

using Lexicon = std::unordered_multimap<string, Lexeme::ptr>;

template <class Input>
Lexeme::ptr parseLexeme(TokenIterator<Input>& it, const int line, Lexicon& lexicon)
{
	const auto get_unique = [&]() -> Lexeme::ptr
	{
		auto key = *it; ++it;
		if (key == "'" && isalpha(it->front()))
		{
			key += *it;
			++it;
		}
		auto range = lexicon.equal_range(key);
		Lexeme::ptr lex;
		if (range.first == range.second)
		{
			lex = std::make_shared<Lexeme>(key);
			lexicon.emplace(key, lex);
		}
		else
		{
			lex = range.first->second;
			++range.first;
		}
		if (range.first == range.second)
			return lex;

		std::cout << "ignoring ambiguous lexeme '" << key << "' referred on line " << line << "\n";
		return nullptr;
	};
	const auto read_dotlist = [&]
	{	
		Lexeme::Or result;
		for (bool more_or = true; !it.skipws(); )
		{
			if (auto lex = get_unique())
			{
				if (std::exchange(more_or, false))
					result.emplace_back();
				result.back().emplace_back(move(lex));
			}
			if (!it || it.isWhitespace() || it.isNewline()) break;
			if (*it == "|")
				more_or = true;
			else if (*it != ".")
			{
				std::cout << "expected '.', '|', or whitespace";
				it.flushLine();
				break;
			}
			++it;
		}
		return result;
	};

	if (it.isNewline() || it.isWhitespace()) ++it;
	if (!it) return nullptr;
	std::shared_ptr<Lexeme> lex = std::make_shared<Lexeme>(*it);
	if ((++it).skipws()) return lex;

	if (*it != ":")
	{
		std::cout << "expected ':' or newline after lexeme name on line " << line << "\n";
		it.flushLine();
		return lex;
	}
	++it;

	while (it && !it.isNewline())
	{
		if (it.skipws()) 
			return lex;
		if (it->size() == 1) switch (it->front())
		{
		case ':': ++it; lex->rels[Rel::spec] = read_dotlist(); continue;
		case '+': ++it; lex->rels[Rel::comp] = read_dotlist(); continue;
		case '*': ++it; lex->rels[Rel::bicomp] = read_dotlist(); continue;
		case '<': ++it; lex->rels[Rel::mod]  = read_dotlist(); continue;
		default: break;
		}
		if (isalnum(it->front()))
		{
			if (auto list = read_dotlist(); !list.empty())
				lex->rels[Rel::is] = move(list);
			continue;
		}
		std::cout << "unexpected relation '" << *it << "' on line " << line << "\n";
		break;
	}
	while (it && !it.isNewline())
	{
		std::cout << "ignoring '" << *it << "' on line " << line << "\n";
		++it;
	}
	return lex;
}




std::vector<Phrase::ptr> parse_word(string_view orth)
{
	static const auto lexicon = [&]
	{
		Lexicon result;
		int line = 1;
		for (TokenIterator<std::ifstream> it("lexemes.txt"); it; ++it, ++line)
		{
			if (auto lex = parseLexeme(it, line, result))
				result.emplace(lex->name, lex);
		}
		line = 1;
		for (TokenIterator<std::ifstream> it("words.txt"); it; ++it, ++line)
		{
			if (auto lex = parseLexeme(it, line, result))
				result.emplace('\'' + lex->name, lex);
		}
		return result;
	}();
	struct OrthParser
	{
		Parser parser;
		uint64_t checked = 0;
		const string_view orth;

		OrthParser(string_view orth) : orth(orth) { }

		void maybe_parse_rest(int from) // assuming this function will be inlined
		{
			if ((checked & (1 << from)) == 0)
				parse_rest(from);
		}

		void parse_rest(int from)
		{
			checked |= (1 << from);
			for (int to = from; to < orth.size(); ++to)
				for (auto&& e : lexicon.equal_range('\'' + string(orth.substr(from, to+1 - from))) | ranged::values)
				{
					parser.insert(std::make_shared<Morpheme>(e), from, to);
					maybe_parse_rest(to+1);
				}
		}

		std::vector<Phrase::ptr> parse()
		{
			assert(orth.size() < sizeof(checked)*8);
			parse_rest(0);

			auto results = parser.run();
			if (results.size() == 1 && parser.length() == orth.size())
				return results.front() | ranged::map([](Phrase::ptr p)
			{
				const auto m = std::dynamic_pointer_cast<const Morpheme>(p);
				assert(m != nullptr);
				return std::make_shared<Word>(m->lex, m);
			});
			return {};
		}
	};

	return OrthParser{ orth }.parse();
}
