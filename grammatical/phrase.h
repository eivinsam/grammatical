#pragma once

#include "ranged.h"

#include <memory>
#include <map>
#include <vector>
#include <string_view>


template <class T>
class Chain
{
	struct Node;
	using NodePtr = std::shared_ptr<const Node>;
	struct Node
	{
		T value;
		NodePtr next;

		template <class... Args>
		Node(NodePtr next, Args&&... args) : value(std::forward<Args>(args)...), next(move(next)) { }
	};
	NodePtr _first;
public:
	using size_type = int;
	using value_type = const T;
	class iterator
	{
		friend class Chain;
		const Node* _node;
		iterator(const Node* node) : _node(node) { }
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = void;
		using value_type = const T;
		using reference = value_type&;
		using pointer = value_type*;

		iterator& operator++() { _node = _node->next.get(); return *this; }

		reference operator*() const { return _node->value; }
		pointer operator->() const { return &_node->value; }

		bool operator!=(const iterator& b) const { return _node != b._node; }
	};
	using const_iterator = iterator;

	int length() const
	{
		int result = 0;
		for (auto next = _first; next; next = next->next)
			++result;
		return result;
	}

	template <class... Args>
	void emplace(Args&&... args)
	{
		_first = std::make_shared<Node>(std::move(_first), std::forward<Args>(args)...);
	}

	const_iterator begin() const { return { _first.get() }; }
	const_iterator end() const { return { nullptr }; }

	Chain operator+(const Chain& b) const
	{
		if (!b._first)
			return *this;
		Chain result = b;
		for (auto&& value : *this)
			result.emplace(value);
		return result;
	}
};

enum class Rel : char { is, spec, mod, comp, bicomp };

class Phrase;
struct Head : public std::shared_ptr<const Phrase> { };
using Mod = std::shared_ptr<const Phrase>;

inline const Head& head(const std::shared_ptr<const Phrase>& p) { return static_cast<const Head&>(p); }

using RuleOutput = std::vector<std::shared_ptr<Phrase>>;
using LeftRule = RuleOutput(*)(const Mod&, const Head&);
using RightRule = RuleOutput(*)(const Head&, const Mod&);

inline RuleOutput no_left(const Mod&, const Head&) { return {}; }
inline RuleOutput no_right(const Head&, const Mod&) { return {}; }

class Lexeme
{
public:
	using string = std::string;
	using ptr = std::shared_ptr<const Lexeme>;
	class And : public std::vector<ptr>
	{
	public:
		bool contains(const ptr& p) const
		{
			for (auto&& e : *this)
				if (e->is(p))
					return true;
			return false;
		}
	};
	class Or : public std::vector<And>
	{
	public:
		bool contains(const ptr& p) const
		{
			for (auto&& e : *this)
				if (e.contains(p))
					return true;
			return false;
		}
	};
private:
	const Or& _get_rel(Rel rel) const
	{
		if (const auto found = rels.find(rel); found != rels.end())
			return found->second;

		static const Or empty;
		return empty;
	}
public:

	string name;

	std::map<Rel, Or> rels;

	Lexeme(string name) : name(move(name)) { }

	const Or& is()   const { return _get_rel(Rel::is); }
	const Or& spec() const { return _get_rel(Rel::spec); }
	const Or& comp() const { return _get_rel(Rel::comp); }
	const Or& mod()  const { return _get_rel(Rel::mod); }

	bool is(std::string_view n) const
	{
		if (name == n)
			return true;
		for (auto&& all : is())
			for (auto&& p : all)
				if (p && p->is(n))
					return true;
		return false;
	}

	struct MatchResult
	{
		bool found;
		bool noMismatch;

		constexpr explicit operator bool() const { return found && noMismatch; }
	};

	template <Rel R>
	MatchResult matches(const ptr& p) const 
	{
		if constexpr (R == Rel::is)
			return { true, this == p.get() || is().contains(p) };

		auto&& prel = p->_get_rel(R);
		auto&& pis = p->is();
		if (!prel.empty())
			return { true, is(prel) && (pis.empty() || matches<R>(pis).noMismatch) };
		else if (!pis.empty())
			return matches<R>(pis);
		else
			return { false, true };
	}
	template <Rel R>
	MatchResult matches(const And& b) const
	{
		bool found = false;
		for (auto&& e : b)
			if (auto match = matches<R>(e); match.noMismatch)
				found |= match.found;
			else
				return match;
		return { found, true };
	}
	template <Rel R>
	MatchResult matches(const Or&  b) const
	{
		bool noMismatch = false;
		for (auto&& e : b)
			if (const auto match = matches<R>(e))
				return match;
			else
				noMismatch |= match.noMismatch;
		return { false, noMismatch };
	}
	bool is(const ptr& p) const { return matches<Rel::is>(p).noMismatch; }
	bool is(const And& b) const { return matches<Rel::is>(b).noMismatch; }
	bool is(const  Or& b) const { return matches<Rel::is>(b).noMismatch; }
};

namespace tag
{
	using namespace std::literals;
	static constexpr auto prep = "prep"sv;
	static constexpr auto verb = "verb"sv;
	static constexpr auto pres = "pres"sv;
	static constexpr auto past = "past"sv;
	static constexpr auto part = "part"sv;
	static constexpr auto free = "free"sv;
	static constexpr auto fin = "fin"sv;
	static constexpr auto gen = "gen"sv;
	static constexpr auto akk = "akk"sv;
	static constexpr auto nom = "nom"sv;
	static constexpr auto adn = "adn"sv;
	static constexpr auto adad = "adad"sv;
}

class Phrase
{
public:
	using string = std::string;
	using ptr = std::shared_ptr<const Phrase>;

	Phrase(int length, Lexeme::ptr lex, Chain<string> errors, LeftRule l, RightRule r) 
		: length(length), lex(move(lex)), errors(move(errors)), left_rule(l), right_rule(r) { }
	Phrase(int length, Lexeme::ptr lex, Chain<string> errors) 
		: length(length), lex(move(lex)), errors(move(errors)) { }
	virtual ~Phrase() = default;

	const int length;

	Lexeme::ptr lex;

	LeftRule left_rule = no_left;
	RightRule right_rule = no_right;

	Chain<string> errors;

	bool is(std::string_view name) const { return lex->is(name); }
	
	virtual bool hasBranch(char type) const = 0;

	virtual string toString() const = 0;
};

class BinaryPhrase : public Phrase
{
public:
	BinaryPhrase(Lexeme::ptr&& lex, char type, Head&& head, Mod&& mod, LeftRule l, RightRule r) :
		Phrase(head->length + mod->length, move(lex), head->errors + mod->errors, l, r), 
		type(type), head(move(head)), mod(move(mod)) { }

	const char type;
	const Head head;
	const Mod mod;

	bool hasBranch(char t) const final { return t == type || head->hasBranch(t); }
};

class LeftBranch : public BinaryPhrase
{
public:
	LeftBranch(Lexeme::ptr lex, char type, Head head, Mod mod, LeftRule l, RightRule r)
		: BinaryPhrase(move(lex), type, move(head), move(mod), l, r) { }

	string toString() const final
	{
		return "[" + mod->toString() + type + ' ' + head->toString() + "]";
	}
};
class RightBranch : public BinaryPhrase
{
public:
	RightBranch(Lexeme::ptr lex, char type, Head head, Mod mod, LeftRule l, RightRule r)
		: BinaryPhrase(move(lex), type, move(head), move(mod), l, r) { }

	string toString() const final
	{
		return "[" + head->toString() + ' ' + type + mod->toString() + "]";
	}
};




class Word : public Phrase
{
public:
	Word(Lexeme::ptr lex);

	bool hasBranch(char) const final { return false; }
	string toString() const final { return lex->name; }
};




std::vector<std::shared_ptr<const Word>> parse_word(std::string_view orth);
