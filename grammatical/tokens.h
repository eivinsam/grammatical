#pragma once

#include "ranged.h"

#include <string>
#include <memory>

template <class Input>
class TokenIterator
{
	std::shared_ptr<Input> _input;
	std::string _token;

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

	const std::string& operator*() const { return _token; }
	const std::string* operator->() const { return &_token; }

	bool operator==(End) const { return _token.empty(); }
	bool operator!=(End) const { return !operator==(End{}); }

	explicit operator bool() const { return !_token.empty(); }

	bool isNewline()    const { return _token.size() == 1 && _token.front() == '\n'; }
	bool isWhitespace() const { return _token.size() == 1 && _token.front() == ' '; }

	void flushLine()
	{
		if (isNewline())
			return;
		while (_input->get() != '\n') {}
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
