#include "phrase.h"
#include "ranged.h"

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

template <class Input>
class TokenIterator
{
	shared_ptr<Input> _input;
	string _token;

	void _read_break()
	{
		_token = "\n";
		for (;;) switch (_input->peek())
		{
		case ' ': case '\t': case '\r': case '\n':
			_input->get();
			continue;
		default:
			return;
		}
	}
	void _read_white()
	{
		_token = " ";
		for (;;) switch (_input->peek())
		{
		case ' ': case '\t': _input->get(); continue;
		case '\r': case '\n': _read_break(); return;
		default:
			return;
		}
	}

	static bool _is_white(char ch) { return ch == ' ' || ch == '\t'; }
	static bool _is_newline(char ch) { return ch == '\r' || ch == '\n'; }

	void _read_token()
	{
		_token.clear();
		switch (const char ch = _input->get())
		{
		case ' ': case '\t': _read_white(); return;
		case '\r': case '\n': _read_break(); return;
		default:
			if (!_input->good())
				return;
			_token.push_back(ch);
			if (isalnum(ch))
			{
				while (isalnum(_input->peek()))
					_token.push_back(_input->get());
			}
			return;
		}
	}
public:
	struct End { };

	template <class... Args>
	TokenIterator(Args&&... args) : _input(std::make_shared<Input>(std::forward<Args>(args)...)) { _read_token(); }

	TokenIterator& operator++() { _read_token(); return *this; }

	const string& operator*() const { return _token; }
	const string* operator->() const { return &_token; }

	bool operator==(End) const { return _token.empty(); }
	bool operator!=(End) const { return !operator==(End{}); }

	explicit operator bool() const { return !_token.empty(); }

	bool isNewline()    const { return _token.size() == 1 && _token.front() == '\n'; }
	bool isWhitespace() const { return _token.size() == 1 && _token.front() == ' '; }

	void flushLine()
	{
		if (isNewline()) 
			return;
		while (_input->get() != '\n') { }
		_read_break();
	}
	bool skipws()
	{
		if (isWhitespace())
			_read_token();
		return _token.empty() || isNewline();
	}
};

template <class Input>
struct std::iterator_traits<TokenIterator<Input>> : ranged::InputInteratorTraits<TokenIterator<Input>> { };

template <class Input, class... Args>
auto tokens(Args&&... args) { return ranged::range(TokenIterator<Input>(std::forward<Args>(args)...), typename TokenIterator<Input>::End{}); }


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
		auto range = lexicon.equal_range(*it);
		Lexeme::ptr lex;
		if (range.first == range.second)
		{
			lex = std::make_shared<Lexeme>(*it);
			lexicon.emplace(*it, lex);
		}
		else
		{
			lex = range.first->second;
			++range.first;
		}
		if (range.first == range.second)
			return lex;

		std::cout << "ignoring ambiguous lexeme '" << *it << "' referred on line " << line << "\n";
		return nullptr;
	};
	const auto write_dotlist_to = [&](auto& dst)
	{
		for (;;)
		{
			if constexpr (std::is_same_v<std::decay_t<decltype(dst.back())>, Lexeme::ptr>)
			{
				if (auto lex = get_unique())
					dst.emplace_back(lex);
			}
			else
				dst.emplace_back(*it);
			++it;
			if (!it || it.isWhitespace() || it.isNewline()) return;
			if (*it != ".")
			{
				std::cout << "expected '.' or whitespace";
				it.flushLine();
				return;
			}
			++it;
		}
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
	if ((++it).skipws()) return lex;

	if (*it != ":") write_dotlist_to(lex->parts);
	if (it.skipws()) return lex;
	if (*it == ":")
	{
		++it;
		if (!it || it.isNewline() | it.isWhitespace())
		{
			std::cout << "unexpected whitespace after ':' on line " << line << "\n";
		}
		else
		{
			write_dotlist_to(lex->spec);
		}
	}

	while (it && !it.isNewline())
	{
		std::cout << "ignoring '" << *it << "' on line " << line << "\n";
		++it;
	}
	return lex;
}


vector<shared_ptr<const Word>> parse_word(string_view orth)
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


	return lexicon.equal_range('\''+string(orth)) | ranged::values | ranged::map(std::make_shared<Word, const Lexeme::ptr&>);
}
