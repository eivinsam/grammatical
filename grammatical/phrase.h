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


enum class Tag : char
{
	suffix, 
	prep, adv, adn, adad, 
	sg, pl, uc, rc, 
	first, second, third, 
	nom, akk, gen,
	pres, past, 
	fin, part, free,
	rsg, rpast, rpart,
	verbe, verby
};

class Tags
{
	unsigned _flags = 0;

	static constexpr unsigned _flag(Tag b) { return 1 << static_cast<unsigned>(b); }

	//static constexpr unsigned _count(unsigned flags)
	//{
	//	flags = ((flags & 0xaaaaaaaa) >> 1) + (flags & 0x55555555);
	//	flags = ((flags & 0xcccccccc) >> 2) + (flags & 0x33333333);
	//	flags = ((flags & 0xf0f0f0f0) >> 4) + (flags & 0x0f0f0f0f);
	//	flags = ((flags & 0xff00ff00) >> 8) + (flags & 0x00ff00ff);
	//	flags = ((flags & 0xffff0000) >> 16) + (flags & 0x0000ffff);
	//	return flags;
	//}

	constexpr Tags(unsigned flags) : _flags(flags) { }

public:
	constexpr Tags() = default;
	constexpr Tags(Tag b) : _flags(_flag(b)) { }

	friend constexpr Tags operator|(Tag a, Tags b) { return { _flag(a) | b._flags }; }
	friend constexpr Tags operator|(Tags a, Tag b) { return { a._flags | _flag(b) }; }
	constexpr Tags operator|(Tags b) const { return { _flags | b._flags }; }

	constexpr bool has(Tag b) const { return hasAny(b); }
	constexpr bool hasAny(Tags b) const { return (_flags&b._flags) != 0; }
	constexpr bool hasAll(Tags b) const { return (_flags&b._flags) == b._flags; }

	constexpr void insert(Tags b) { _flags |=  b._flags; }
	constexpr void remove(Tags b) { _flags &= ~b._flags; }

	constexpr Tags select(Tags b) const { return { _flags & b._flags }; }

	constexpr explicit operator bool() const { return _flags != 0; }
};

constexpr Tags operator|(Tag a, Tag b) { return Tags{} | a | b; }

namespace tags
{
	static constexpr Tags number = Tag::sg | Tag::pl | Tag::uc;
	static constexpr Tags person = Tag::first | Tag::second | Tag::third;
	static constexpr Tags sg3 = Tag::sg | Tag::third;
	static constexpr Tags verb_regularity = Tag::rsg | Tag::rpast | Tag::rpart;
}

enum class Rel : char { spec, mod, comp, bicomp };

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

	class All : public std::vector<ptr>
	{
	public:
		using std::vector<ptr>::vector;
		All() = default;
		All(std::vector<ptr>&& b) : std::vector<ptr>(move(b)) { }
		All(const std::vector<ptr>& b) : std::vector<ptr>(b) { }
	};

	class Any : public std::vector<ptr>
	{
	public:
		using std::vector<ptr>::vector;
		Any() = default;
		Any(std::vector<ptr>&& b) : std::vector<ptr>(move(b)) { }
		Any(const std::vector<ptr>& b) : std::vector<ptr>(b) { }


		bool contains(const ptr& p) const
		{
			for (auto&& e : *this)
				if (e->is(p))
					return true;
			return false;
		}
	};
private:
	const Any& _get_rel(Rel rel) const
	{
		if (const auto found = rels.find(rel); found != rels.end())
			return found->second;

		static const Any empty;
		return empty;
	}

	bool _is_sem(const ptr& p) const
	{
		if (this == p.get())
			return true;
		for (auto&& ep : p->sem)
		{
			for (auto&& e : sem)
				if (e->_is_sem(ep))
					continue;
			return false;
		}
		return true;
	}

public:
	string name;

	Tags syn;
	All  sem;

	std::map<Rel, Any> rels;

	Lexeme(string name) : name(move(name)) { }

	void remove(Tags b) { syn.remove(b); }

	void become(Tags b) { syn.insert(b); }
	void become(const ptr& p)
	{
		become(p->syn);
		if (!p->sem.empty() || !p->rels.empty())
			sem.emplace_back(p);
	}
	void become(const std::vector<ptr>& b)
	{
		sem.reserve(sem.size() + b.size());
		for (auto&& e : b)
			become(e);
	}

	const Any& spec() const { return _get_rel(Rel::spec); }
	const Any& comp() const { return _get_rel(Rel::comp); }
	const Any& mod()  const { return _get_rel(Rel::mod); }

	bool is(const ptr& p) const { return syn.hasAll(p->syn) && _is_sem(p); }
	bool is(const Any& p) const 
	{
		for (auto&& e : p)
			if (is(e))
				return true;
		return false;
	}

	template <Rel R>
	bool matches(const ptr& p) const 
	{
		auto&& prel = p->_get_rel(R);
		if (!prel.empty() && !is(prel))
			return false;
		if (!p->sem.empty())
			return matches<R>(p->sem);
		return true;
	}
	template <Rel R>
	bool matches(const All& b) const
	{
		for (auto&& e : b)
			if (!matches<R>(e))
				return false;
		return true;
	}
	template <Rel R>
	bool matches(const Any& b) const
	{
		for (auto&& e : b)
			if (matches<R>(e))
				return true;
		return false;
	}
};


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

	bool has(Tag b) const { return lex->syn.has(b); }
	bool hasAny(Tags b) const { return lex->syn.hasAny(b); }
	bool hasAll(Tags b) const { return lex->syn.hasAll(b); }

	struct AG
	{
		Tags tags;
		bool with(const Phrase& b) const { return b.hasAll(tags); }
	};
	AG agreesOn(Tags tags) const { return { lex->syn.select(tags) }; }
	
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




class Morpheme : public Phrase
{
public:
	Morpheme(Lexeme::ptr lex);

	bool hasBranch(char) const final { return false; }
	string toString() const final { return lex->name; }
};

class Word : public Phrase
{
	Phrase::ptr _morph;
public:
	Word(Lexeme::ptr lex, Phrase::ptr morph);

	bool hasBranch(char) const final { return false; }
	string toString() const final { return _morph->toString(); }
};




std::vector<Phrase::ptr> parse_word(std::string_view orth);
