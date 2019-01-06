#pragma once

#include "ranged.h"

#include <cassert>
#include <memory>
#include <map>
#include <unordered_map>
#include <vector>
#include <string_view>

template <class P, class IT, class S = IT>
class SelectIterator
{
	P _pred;
	IT _it;
	S _end;

	void _find_match()
	{
		while (_it != _end && !_pred(*_it))
			++_it;
	}
public:
	using iterator_category = std::forward_iterator_tag;
	using difference_type = void;
	using value_type = typename std::iterator_traits<IT>::value_type;
	using reference = typename std::iterator_traits<IT>::reference;
	using pointer = typename std::iterator_traits<IT>::pointer;

	SelectIterator(P pred, IT it, S end) : _pred(std::move(pred)), _it(std::move(it)), _end(std::move(end)) { _find_match(); }

	SelectIterator& operator++()
	{
		if (_it != _end)
		{
			++_it;
			_find_match();
		}
		return *this;
	}

	reference operator*() { return *_it; }
	pointer operator->() { return &*_it; }

	bool operator==(const S& b) const { return _it == b; }
	bool operator!=(const S& b) const { return _it != b; }
};


template <class T>
class Bag
{
	std::vector<T> _data;
public:
	using size_type = typename std::vector<T>::size_type;
	using iterator = typename std::vector<T>::iterator;
	using const_iterator = typename std::vector<T>::const_iterator;

	template <class... Args>
	T& emplace(Args&&... args) { return _data.emplace_back(std::forward<Args>(args)...); }

	template <class P>
	void erase(P&& pred)
	{
		for (size_t i = 0; i < _data.size(); )
		{
			auto& e = _data[i];
			if (pred(e))
			{
				if (i+1 < _data.size())
					e = std::move(_data.back());
				_data.pop_back();
				continue;
			}
			++i;
		}
	}
	template <class P>
	Bag<T> extract(P&& pred)
	{
		Bag<T> result;
		for (size_t i = 0; i < _data.size(); )
		{
			auto& e = _data[i];
			if (pred(e))
			{
				result.emplace(std::move(e));
				if (i+1 < _data.size())
					e = std::move(_data.back());
				_data.pop_back();
				continue;
			}
			++i;
		}
		return result;
	}

	bool empty() const { return _data.empty(); }

	size_type size() const { return _data.size(); }

	iterator begin() { return _data.begin(); }
	iterator end() { return _data.end(); }

	const_iterator begin() const { return _data.begin(); }
	const_iterator end() const { return _data.end(); }

	template <class P>
	auto select(P&& pred) { return ranged::range(SelectIterator<P, iterator>(std::move(pred), begin(), end()), end()); }
	template <class P>
	auto select(P&& pred) const { return ranged::range(SelectIterator<P, const_iterator>(std::move(pred), begin(), end()), end()); }
};


enum class Tag : char
{
	suffix, 
	prep, adv, adn, adad, 
	sg, pl, uc, rc, 
	first, second, third, 
	nom, akk, gen,
	pres, past, dict, modal, 
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
	template <class First, class... Rest>
	constexpr Tags(First first, Rest... rest) : Tags(first) { insert(Tags(rest...)); }

	constexpr bool has(Tag b) const { return hasAny(b); }
	constexpr bool hasAny(Tags b) const { return (_flags&b._flags) != 0; }
	constexpr bool hasAll(Tags b) const { return (_flags&b._flags) == b._flags; }

	constexpr void insert(Tags b) { _flags |=  b._flags; }
	constexpr void remove(Tags b) { _flags &= ~b._flags; }

	constexpr Tags select(Tags b) const { return { _flags & b._flags }; }

	constexpr explicit operator bool() const { return _flags != 0; }
};

namespace tags
{
	inline constexpr Tags number{ Tag::sg, Tag::pl, Tag::uc };
	inline constexpr Tags person{ Tag::first, Tag::second, Tag::third };
	inline constexpr Tags    sg3{ Tag::sg, Tag::third };
	inline constexpr Tags nonsg3{ Tag::pl, Tag::first, Tag::second };
	inline constexpr Tags verb_regularity{ Tag::rsg, Tag::rpast, Tag::rpart };

	static constexpr Tags verbrsg{ Tag::pres, Tag::dict, Tag::fin, nonsg3, Tag::rsg };
	static constexpr Tags verbr{ verbrsg, Tag::rpast, Tag::rpart };
}

enum class Rel : char { spec, mod, comp, bicomp };
enum class Mark : char { None, By, Of, To, For };

inline std::optional<Mark> mark(std::string_view n)
{
	using namespace std::string_view_literals;
	static const std::unordered_map<std::string_view, Mark> lookup =
	{
		{ "none"sv, Mark::None },
		{ "by"sv, Mark::By },
		{ "of"sv, Mark::Of },
		{ "to"sv, Mark::To },
		{ "for"sv, Mark::For }
	};
	if (auto found = lookup.find(n); found != lookup.end())
		return found->second;
	return {};
}

class Phrase;
class BinaryPhrase;
struct Head : public std::shared_ptr<const Phrase> { };
using Mod = std::shared_ptr<const Phrase>;

inline const Head& head(const std::shared_ptr<const Phrase>& p) { return static_cast<const Head&>(p); }

template <class T>
class NonNull
{
	T _ptr;
public:
	constexpr NonNull(T ptr) : _ptr(std::move(ptr)) { if (_ptr == nullptr) throw std::logic_error("nullptr not allowed"); }

	constexpr decltype(auto) operator->() const { return _ptr.operator->(); }
	constexpr decltype(auto) operator*() const { return *_ptr; }

	constexpr bool operator==(const NonNull& other) const { return _ptr == other._ptr; }
	constexpr bool operator!=(const NonNull& other) const { return _ptr != other._ptr; }
};

using RuleOutput = std::vector<std::shared_ptr<Phrase>>;
using RawLeftRule = RuleOutput(*)(const Mod&, const Head&);
using RawRightRule = RuleOutput(*)(const Head&, const Mod&);
class LeftRule : public NonNull<RawLeftRule>
{
public:
	using NonNull<RawLeftRule>::NonNull;

	RuleOutput operator()(const Mod& mod, const Head& head) const { return operator*()(mod, head); }
};
class RightRule : public NonNull<RawRightRule>
{
public:
	using NonNull<RawRightRule>::NonNull;

	RuleOutput operator()(const Head& head, const Mod& mod) const { return operator*()(head, mod); }
};


inline RuleOutput no_left(const Mod&, const Head&) { return {}; }
inline RuleOutput no_right(const Head&, const Mod&) { return {}; }

class Lexeme;
struct Argument
{
	Rel rel;
	Mark mark;
	std::shared_ptr<const Lexeme> sem;

	Argument(Rel rel, Mark mark, std::shared_ptr<const Lexeme> sem) : rel(rel), mark(mark), sem(move(sem)) { }
};

class Lexeme
{
public:
	using string = std::string;
	using ptr = std::shared_ptr<const Lexeme>;
	using ptr_vector = std::vector<ptr>;

	class All : public ptr_vector
	{
	public:
		using ptr_vector::vector;
		All() = default;
		All(ptr_vector&& b) : ptr_vector(move(b)) { }
		All(const ptr_vector& b) : ptr_vector(b) { }
	};

	class Any : public ptr_vector
	{
	public:
		using ptr_vector::vector;
		Any() = default;
		Any(ptr_vector&& b) : ptr_vector(move(b)) { }
		Any(const ptr_vector& b) : ptr_vector(b) { }


		bool contains(const ptr& p) const
		{
			for (auto&& e : *this)
				if (e->is(p))
					return true;
			return false;
		}
	};
private:
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

	All  sem;

	Bag<Argument> args;

	Lexeme(string name) : name(move(name)) { }

	template <class T>
	void update(T&& value)
	{
		assert(!value.syn);
		assert(value.sem);
		if (value.sem->name == "")
			sem = std::move(value.sem->sem);
		else
			sem = { std::move(value.sem) };
	}

	void become(const ptr& p)
	{
		sem.emplace_back(p);
	}
	void become(const ptr_vector& b)
	{
		sem.reserve(sem.size() + b.size());
		for (auto&& e : b)
			become(e);
	}

	bool is(const ptr& p) const { return _is_sem(p); }
	bool is(const Any& p) const 
	{
		for (auto&& e : p)
			if (is(e))
				return true;
		return false;
	}

	template <class C>
	bool matchesAny(const C& c) const
	{
		for (auto&& e : c)
			if (is(e))
				return true;
		return false;
	}
};

class Phrase
{
public:
	using string = std::string;
	using ptr = std::shared_ptr<const Phrase>;
	using mut_ptr = std::shared_ptr<Phrase>;

	Phrase(int length, Tags syn, Lexeme::ptr lex, LeftRule l = no_left, RightRule r = no_right) 
		: length(length), syn(syn), sem(move(lex)), left_rule(l), right_rule(r) { }
	virtual ~Phrase() = default;

	const int length;

	Tags syn;
	Lexeme::ptr sem;
	Bag<Argument> args;


	LeftRule left_rule = no_left;
	RightRule right_rule = no_right;

	std::vector<std::string> errors;

	struct AG
	{
		Tags tags;
		bool with(const Phrase& b) const { return b.syn.hasAll(tags); }
	};
	AG agreesOn(Tags tags) const { return { syn.select(tags) }; }

	virtual size_t errorCount() const = 0;
	
	virtual bool hasBranch(char type) const { return false; }
	virtual const BinaryPhrase* getBranch(char type) const { return nullptr; }

	virtual string toString() const = 0;
};

class BinaryPhrase : public Phrase
{
public:
	BinaryPhrase(Tags syn, Lexeme::ptr&& lex, char type, Head&& head, Mod&& mod, LeftRule l, RightRule r) :
		Phrase(head->length + mod->length, syn, move(lex), l, r),
		type(type), head(move(head)), mod(move(mod)) { }

	const char type;
	const Head head;
	const Mod mod;

	size_t errorCount() const final { return errors.size() + head->errorCount() + mod->errorCount(); }

	bool hasBranch(char t) const final { return t == type || head->hasBranch(t); }
	const BinaryPhrase* getBranch(char t) const final { return t == type ? this : head->getBranch(t); }
};

class LeftBranch : public BinaryPhrase
{
public:
	LeftBranch(Tags syn, Lexeme::ptr lex, char type, Head head, Mod mod, LeftRule l, RightRule r)
		: BinaryPhrase(syn, move(lex), type, move(head), move(mod), l, r) { }

	string toString() const final
	{
		return "[" + mod->toString() + type + ' ' + head->toString() + "]";
	}
};
class RightBranch : public BinaryPhrase
{
public:
	RightBranch(Tags syn, Lexeme::ptr lex, char type, Head head, Mod mod, LeftRule l, RightRule r)
		: BinaryPhrase(syn, move(lex), type, move(head), move(mod), l, r) { }

	string toString() const final
	{
		return "[" + head->toString() + ' ' + type + mod->toString() + "]";
	}
};




class Morpheme : public Phrase
{
	void _add_args(const Lexeme& s);
public:
	string orth;

	Morpheme(string orth) : Phrase{ orth.size(),{},{} }, orth(move(orth)) { }

	template <class S>
	void update(S&& s) { update(s.syn, std::move(s.sem)); }
	void update(Tags syn, Lexeme::ptr sem);

	size_t errorCount() const final { return errors.size(); }

	string toString() const final { return orth; }
};

class Word : public Phrase
{
	Phrase::ptr _morph;
public:
	Word(Lexeme::ptr lex, Phrase::ptr morph);

	size_t errorCount() const { return errors.size() + _morph->errorCount(); }

	string toString() const final { return _morph->toString(); }
};




std::vector<Phrase::ptr> parse_word(std::string_view orth);
