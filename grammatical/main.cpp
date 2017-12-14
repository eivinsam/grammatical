#include "phrase.h"
#include "tokens.h"

#include <memory>
#include <iostream>
#include <sstream>

#include <queue>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>
#include <functional>
#include <bitset>

using std::pair;
using std::string;
using std::string_view;
using std::initializer_list;
using std::vector;
using std::optional;
using std::nullopt;
using std::shared_ptr;
using std::make_shared;
using std::move;
using std::forward;
using std::cout;



class Parser
{
	using PhrasePtr = shared_ptr<const Phrase>;

	struct Position
	{
		vector<PhrasePtr> begins_with;
		vector<PhrasePtr> ends_with;
	};
	vector<Position> _positions;
	vector<PhrasePtr> _top;

	struct Item
	{
		PhrasePtr phrase;
		int from;
		int to;

		Item(PhrasePtr phrase, int from, int to) : phrase(move(phrase)), from(from), to(to) { }
	};
	struct ErrorOrder
	{
		bool operator()(const Item& a, const Item& b)
		{
			return a.phrase->errors.length() > b.phrase->errors.length();
		}
	};
	std::priority_queue<Item, vector<Item>, ErrorOrder> _agenda;

	void _match(const shared_ptr<const Phrase>& a, const shared_ptr<const Phrase>& b, int from, int to)
	{
		for (auto match : a->right_rule(head(a), b))
			_agenda.emplace(move(match), from, to);
		for (auto match : b->left_rule(a, head(b)))
			_agenda.emplace(move(match), from, to);
	}

	void _generate_result(int length, vector<PhrasePtr>&& so_far, vector<vector<PhrasePtr>>& result)
	{
		if (length == _positions.size())
			result.emplace_back(move(so_far));
		else
		{
			int longest = 0;
			for (auto&& p : _positions[length].begins_with)
				if (longest < p->length)
					longest = p->length;
			if (longest > 0)
				for (auto&& p : _positions[length].begins_with) if (p->length == longest)
				{
					auto copy = so_far;
					copy.emplace_back(p);
					_generate_result(length + longest, move(copy), result);
				}
		}
	}
public:

	void push(shared_ptr<const Phrase> p)
	{
		const int i = int(_positions.size());
		_positions.emplace_back();
		_agenda.emplace(move(p), i, i);
		_top.clear();
	}
	void push(const vector<shared_ptr<const Word>>& alternatives)
	{
		const int i = int(_positions.size());
		_positions.emplace_back();
		_top.clear();
		for (auto&& p : alternatives)
			_agenda.emplace(p, i, i);
	}

	vector<vector<PhrasePtr>> run()
	{
		while (!_agenda.empty())
		{
			if (!_top.empty() && _agenda.top().phrase->errors.length() > _top.front()->errors.length())
				break;
			auto item = _agenda.top(); _agenda.pop();
			_positions[item.from].begins_with.emplace_back(item.phrase);
			_positions[item.to].ends_with.emplace_back(item.phrase);

			if (item.phrase->length == int(_positions.size()))
				_top.emplace_back(item.phrase);

			if (item.from > 0)
				for (auto&& e : _positions[item.from - 1].ends_with)
					_match(e, item.phrase, item.from - e->length, item.to);
			if (item.to + 1 < int(_positions.size()))
				for (auto&& e : _positions[item.to + 1].begins_with)
					_match(item.phrase, e, item.from, item.to + e->length);
		}

		vector<vector<PhrasePtr>> result;

		_generate_result(0, {}, result);

		return result;
	}
};

template <class Stream>
class Tokenizer
{
	TokenIterator<Stream> _it;
public:
	template <class... Args>
	Tokenizer(Args&&... args) : _it(forward<Args>(args)...) { }

	optional<vector<shared_ptr<const Word>>> next()
	{
		if (_it.isWhitespace()) ++_it;
		if (!_it || _it.isNewline())
			return nullopt;
		auto result = parse_word(*_it);
		if (result.empty())
		{
			auto new_word = make_shared<Word>(make_shared<Lexeme>(*_it));
			new_word->errors.emplace("unknown word " + *_it);
			result.emplace_back(move(new_word));
		}
		++_it;
		return result;
	}
};

// 'I  -> noun, sg, 1, nom
// 'me -> noun, sg, 1, acc
// 'my -> noun, sg, 1, gen
// ...
// 'cow -> noun, 3, nom, acc, :sg(gen)
// 'cows -> noun, 3, nom, acc, pl, :pl(gen)
// 'milk -> noun, 3, nom, acc, sg, :(gen)

// X(Y) + Z:X(Y) -> Z.X

// gen.sg + noun:gen.sg -> noun.sg
// gen.pl + noun:gen.pl -> noun.pl
// noun.nom.nonsg3 + verb.pres:nom.nonsg3 -> verb.pres.s
// noun.nom.sg.3 + verb.pres:nom.sg.3 -> verb.pres.s
// noun.nom.num.per + verb.past:nom -> verb.past.s
// noun.nom.num.par + verb.part:nom -> verb.part.s
// 

int main(int argc, char* argv[])
{
	const char* sentences[] =
	{
		"the book",
		"that English teacher",
		"a wish",
		"my latest idea",
		"computers are very expensive",
		"do you sell old books"
	};

	int i = 0;
	for (auto sentence : sentences)
	{
		++i;
		Tokenizer<std::istringstream> tokens(sentence);

		Parser parse;

		while (auto word = tokens.next())
			parse.push(*word);

		for (auto&& result : parse.run())
		{
			cout << i << ":";
			for (auto&& phrase : result)
				cout << ' ' << phrase->toString();
			cout << '\n';
			for (auto&& phrase : result)
				for (auto&& error : phrase->errors)
					cout << "  * " << error << '\n';
		}
	}

	return 0;
}

