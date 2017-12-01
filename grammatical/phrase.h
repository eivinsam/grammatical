#pragma once

#include <memory>
#include <vector>

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



class Phrase;
using RuleInput = const std::shared_ptr<const Phrase>&;
using RuleOutput = std::shared_ptr<Phrase>;
using Rule = RuleOutput(*)(RuleInput, RuleInput);

inline RuleOutput no_rule(RuleInput, RuleInput) { return nullptr; }

enum class Tag : char 
{ 
	verb, noun, prep, adv, adn, adad, det, 
	sg, pl, uc, first, second, third, 
	past, finite, part, aux_past, aux_pres, 
	spec, od, oi
};

enum class Branch : char { left, right };

namespace tags
{
	struct With;
	struct On;
}

class Tags
{
	unsigned _flags = 0;

	static constexpr unsigned _flag(Tag b) { return 1 << static_cast<unsigned>(b); }

	static constexpr unsigned _count(unsigned flags)
	{
		flags = ((flags & 0xaaaaaaaa) >> 1) + (flags & 0x55555555);
		flags = ((flags & 0xcccccccc) >> 2) + (flags & 0x33333333);
		flags = ((flags & 0xf0f0f0f0) >> 4) + (flags & 0x0f0f0f0f);
		flags = ((flags & 0xff00ff00) >> 8) + (flags & 0x00ff00ff);
		flags = ((flags & 0xffff0000) >> 16) + (flags & 0x0000ffff);
		return flags;
	}

	constexpr Tags(unsigned flags) : _flags(flags) { }

public:
	constexpr Tags() = default;
	constexpr Tags(Tag b) : _flags(_flag(b)) { }

	friend constexpr Tags operator|(Tag a, Tags b) { return { _flag(a) | b._flags }; }
	friend constexpr Tags operator|(Tags a, Tag b) { return { a._flags | _flag(b) }; }
	constexpr Tags operator|(Tags b) const { return { _flags | b._flags }; }

	constexpr bool has(Tags b) const { return (_flags&b._flags) != 0; }

	constexpr Tags agreement(tags::With with, tags::On on) const; 

	constexpr explicit operator bool() const { return _flags != 0; }
};

constexpr Tags operator|(Tag a, Tag b) { return Tags{} | a | b; }

namespace tags
{
	static constexpr Tags none = {};
	static constexpr Tags number = Tag::sg | Tag::pl;

	struct With
	{
		Tags tags;
	};
	struct On
	{
		Tags tags;
	};
}

constexpr Tags Tags::agreement(tags::With with, tags::On on) const
{
	return { (_flags&~on.tags._flags) | (_flags & on.tags._flags & with.tags._flags) };
}

inline constexpr tags::With with(Tags tags) { return { tags }; }
inline constexpr tags::On     on(Tags tags) { return { tags }; }


class Phrase
{
public:
	using string = std::string;
	using ptr = std::shared_ptr<const Phrase>;

	Phrase(int length, Tags tags, Chain<string> errors, Rule l, Rule r) : length(length), tags(tags), errors(move(errors)), left_rule(l), right_rule(r) { }
	Phrase(int length, Tags tags, Chain<string> errors) : length(length), tags(tags), errors(move(errors)) { }
	virtual ~Phrase() = default;

	const int length;

	Tags tags;

	Rule left_rule = no_rule;
	Rule right_rule = no_rule;

	Chain<string> errors;

	bool has(Tags b) const { return tags.has(b); }

	virtual string toString() const = 0;
};

class BinaryPhrase : public Phrase
{
public:
	BinaryPhrase(Tags tags, Phrase::ptr head, Branch branch, Phrase::ptr mod, Rule l, Rule r) :
		Phrase(head->length + mod->length, tags, head->errors + mod->errors, l, r), head(move(head)), mod(move(mod)), branch(branch) { }

	const Phrase::ptr head;
	const Phrase::ptr mod;
	const Branch branch;

	string toString() const final
	{
		return branch == Branch::right ?
			"[" + head->toString() + "< " + mod->toString() + "]" :
			"[" + mod->toString() + " >" + head->toString() + "]";
	}
};

class Word : public Phrase
{
public:
	const string orth;

	Word(string orth, Tags tags);
	Word(std::string_view orth, Tags tags) : Word(string(orth), tags) { }

	string toString() const final { return orth; }
};




const std::vector<std::shared_ptr<const Word>>& parse_word(std::string_view orth);
