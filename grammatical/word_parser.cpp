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

template <class C, class T>
auto find(C&& c, const T& value) -> std::optional<decltype(c.find(value)->second)>
{
	if (auto found = c.find(value); found != c.end())
		return found->second;
	return {};
}

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

struct Data
{
	std::unordered_multimap<string, Lexeme::ptr> lexicon;
	std::unordered_multimap<string, Phrase::ptr> dictionary;

	using Input = std::ifstream;


	auto get_lex(const string& key) const
	{
		auto range = lexicon.equal_range(key);
		if (range.first == range.second)
			throw std::runtime_error("lexeme '" + key + "' not found");

		Lexeme::ptr lex = range.first->second;

		if (++range.first != range.second)
			throw std::runtime_error("ignoring ambiguous lexeme '" + key + "' referred");

		return lex;
	}

	auto read_dotlist(TokenIterator<Input>& it) const
	{
		static const std::unordered_map<string, Tags> tag_lookup =
		{
			{ "suffix", Tag::suffix },
		{ "prep", Tag::prep },
		{ "adv", Tag::adv },{ "adn", Tag::adn },{ "adad", Tag::adad },
		{ "sg", Tag::sg },{ "pl", Tag::pl },{ "uc", Tag::uc },{ "rc", Tag::rc },
		{ "1", Tag::first },{ "2", Tag::second },{ "3", Tag::third },
		{ "nom", Tag::nom },{ "akk", Tag::akk },{ "gen", Tag::gen },
		{ "pres", Tag::pres },{ "past", Tag::past },
		{ "part", Tag::part },{ "fin", Tag::fin },
		{ "rsg", Tag::rsg },{ "rpast", Tag::rpast },{ "rpart", Tag::rpart },
		{ "verbe", Tag::verbe },{ "verby", Tag::verby },
		{ "verbrsg", tags::verbrsg }, { "verbr", tags::verbr }
		};
		struct
		{
			Tags syn;
			Lexeme::ptr sem;
		} result;
		shared_ptr<Lexeme> meta;
		for (;; ++it)
		{
			string key = *it; ++it;
			if (auto value = find(tag_lookup, key))
			{
				result.syn.insert(*value);
			}
			else if (!result.sem)
			{
				result.sem = get_lex(key);
			}
			else
			{
				if (!meta)
				{
					meta = std::make_shared<Lexeme>("");
					meta->become(move(result.sem));
					result.sem = meta;
				}
				meta->become(get_lex(key));
			}
			if (*it != ".")
				return result;
		}
	}

	auto read_pipelist(TokenIterator<Input>& it) const 
	{
		vector<Lexeme::ptr> result;
		for (;; ++it)
		{
			auto dotlist = read_dotlist(it);
			assert(!dotlist.syn);
			assert(dotlist.sem);
			result.emplace_back(move(dotlist.sem));
			if (*it != "|")
				return result;
		}
	}

	void parse_arg(Rel rel, TokenIterator<Input>& it, const shared_ptr<Morpheme>& m) const
	{
		for (auto&& l : read_pipelist(it))
			m->args.emplace(rel, l);
	}

	shared_ptr<Morpheme> parseMorpheme(TokenIterator<Input>& it, const int line)
	{

		if (it.isNewline() || it.isWhitespace()) ++it;
		if (!it) return nullptr;
		const auto m = std::make_shared<Morpheme>(*it);
		if ((++it).skipws()) return m;

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
					return m;
				if (it->size() == 1) switch (it->front())
				{
				case ':': ++it; parse_arg(Rel::spec,   it, m); continue;
				case '+': ++it; parse_arg(Rel::comp,   it, m); continue;
				case '*': ++it; parse_arg(Rel::bicomp, it, m); continue;
				case '<': ++it; parse_arg(Rel::mod,    it, m); continue;
				default:
					break;
				}
				if (isalnum(it->front()))
				{
					m->update(read_dotlist(it));
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
			std::cout << e.what() << " while reading '" << m->orth << "' on line " << line << "\n";
		}
		return m;
	}

};


std::vector<Phrase::ptr> parse_word(string_view orth)
{
	static const auto data = [&]
	{
		Data result;
		int line = 1;
		//for (TokenIterator<std::ifstream> it("lexemes.txt"); it; ++it, ++line)
		//{
		//	if (auto lex = parseLexeme(it, line, result))
		//		result.emplace(lex->name, lex);
		//}
		//line = 1;
		for (TokenIterator<std::ifstream> it("words.txt"); it; ++it, ++line)
		{
			if (auto m = result.parseMorpheme(it, line))
				result.dictionary.emplace(m->orth, m);
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
				for (auto&& e : data.dictionary.equal_range(string(orth.substr(from, to+1 - from))) | ranged::values)
				{
					parser.insert(e, from, to);
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
						result.emplace_back(std::make_shared<Word>(r.front()->sem, r.front()));
				return result;
			}
			return {};
		}
	};

	return OrthParser{ orth }.parse();
}
