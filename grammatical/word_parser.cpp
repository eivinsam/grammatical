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

class Lexicon : public std::unordered_multimap<string, Lexeme::ptr>
{
public:
	Lexicon(std::initializer_list<std::pair<const char*, Tags>> init)
	{
		for (auto&& p : init)
		{
			auto lex = std::make_shared<Lexeme>(p.first);
			lex->become(p.second);
			emplace(p.first, move(lex));
		}
	}
};

template <class Input> 
static auto read_dotlist(TokenIterator<Input>& it, const Lexicon& lexicon)
{
	vector<Lexeme::ptr> result;
	for (;; ++it)
	{
		string key = *it; ++it;
		if (key == "'" && isalpha(it->front()))
		{
			key += *it;
			++it;
		}
		auto range = lexicon.equal_range(key);
		if (range.first == range.second)
			throw std::runtime_error("lexeme '" + key + "' not found");

		Lexeme::ptr lex = range.first->second;

		if (++range.first != range.second)
			throw std::runtime_error("ignoring ambiguous lexeme '" + key + "' referred");

		result.emplace_back(move(lex));
		if (*it != ".")
			return result;
	}
}

template <class Input>
static auto read_pipelist(TokenIterator<Input>& it, const Lexicon& lexicon)
{
	vector<Lexeme::ptr> result;
	for (;; ++it)
	{
		auto dotlist = read_dotlist(it, lexicon);
		assert(dotlist.size() > 0);
		if (dotlist.size() == 1)
			result.emplace_back(move(dotlist.front()));
		else
		{
			auto meta = std::make_shared<Lexeme>("");
			meta->become(move(dotlist));
			result.emplace_back(move(meta));
		}
		if (*it != "|")
			return result;
	}
}

template <class Input>
Lexeme::ptr parseLexeme(TokenIterator<Input>& it, const int line, Lexicon& lexicon)
{
	if (it.isNewline() || it.isWhitespace()) ++it;
	if (!it) return nullptr;
	const auto lex = std::make_shared<Lexeme>(*it);
	if ((++it).skipws()) return lex;


	try
	{
		if (*it != ":")
		{
			it.flushLine();
			throw std::runtime_error("expected ':' or newline after lexeme name");
		}
		++it;

		while (it && !it.isNewline())
		{
			if (it.skipws())
				return lex;
			if (it->size() == 1) switch (it->front())
			{
			case ':': ++it; lex->rels[Rel::spec]   = read_pipelist(it, lexicon); continue;
			case '+': ++it; lex->rels[Rel::comp]   = read_pipelist(it, lexicon); continue;
			case '*': ++it; lex->rels[Rel::bicomp] = read_pipelist(it, lexicon); continue;
			case '<': ++it; lex->rels[Rel::mod]    = read_pipelist(it, lexicon); continue;
			default:
				break;
			}
			if (isalnum(it->front()))
			{
				lex->become(read_dotlist(it, lexicon));
				continue;
			}
			throw std::runtime_error("unexpected relation '" + *it + "'");
			break;
		}
		while (it && !it.isNewline())
		{
			std::cout << "ignoring '" << *it << "' on line " << line << "\n";
			++it;
		}
	}
	catch (std::runtime_error& e)
	{
		std::cout << e.what() << " while reading '" << lex->name << "' on line " << line << "\n";
	}
	return lex;
}




std::vector<Phrase::ptr> parse_word(string_view orth)
{
	static const auto lexicon = [&]
	{
		Lexicon result =
		{
			{ "suffix", Tag::suffix },
			{ "prep", Tag::prep },
			{ "adv", Tag::adv },{ "adn", Tag::adn },{ "adad", Tag::adad },
			{ "sg", Tag::sg },{ "pl", Tag::pl },{ "uc", Tag::uc },{ "rc", Tag::rc },
			{ "1", Tag::first },{ "2", Tag::second },{ "3", Tag::third },
			{ "nom", Tag::nom },{ "akk", Tag::akk },{ "gen", Tag::gen },
			{ "pres", Tag::pres },{ "past", Tag::past },
			{ "part", Tag::part },{ "fin", Tag::fin }, 
			{ "rsg", Tag::rsg }, { "rpast", Tag::rpast }, { "rpart", Tag::rpart },
			{ "verbe", Tag::verbe }, { "verby", Tag::verby }
		};
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
			if (parser.length() == orth.size())
			{
				std::vector<Phrase::ptr> result;
				result.reserve(results.size());
				for (auto&& r : results)
					if (r.size() == 1)
						result.emplace_back(std::make_shared<Word>(r.front()->lex, r.front()));
				return result;
			}
			return {};
		}
	};

	return OrthParser{ orth }.parse();
}
