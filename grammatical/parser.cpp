#include "parser.h"
#include <cassert>

void Parser::_match(const Phrase::ptr & a, const Phrase::ptr & b, int from, int to)
{
	for (auto match : a->right_rule(head(a), b))
		_agenda.emplace(move(match), from, to);
	for (auto match : b->left_rule(a, head(b)))
		_agenda.emplace(move(match), from, to);
}

void Parser::_generate_result(int length, Phrases && so_far, std::vector<Phrases>& result)
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

void Parser::push(Phrase::ptr p)
{
	const int i = int(_positions.size());
	_positions.emplace_back();
	_agenda.emplace(move(p), i, i);
	_top.clear();
}

void Parser::push(const Phrases & alternatives)
{
	const int i = int(_positions.size());
	_positions.emplace_back();
	_top.clear();
	for (auto&& p : alternatives)
		_agenda.emplace(p, i, i);
}

void Parser::insert(Phrase::ptr p, int from, int to)
{
	assert(from >= 0);
	assert(to >= from);
	if (_positions.size() <= to)
	{
		_positions.resize(to + 1);
		_top.clear();
	}
	_agenda.emplace(std::move(p), from, to);
}

std::vector<Parser::Phrases> Parser::run()
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

	std::vector<Phrases> result;

	_generate_result(0, {}, result);

	return result;
}
