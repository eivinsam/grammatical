#pragma once

#include "phrase.h"
#include <queue>

class Parser
{
	using Phrases = std::vector<Phrase::ptr>;
	struct Position
	{
		Phrases begins_with;
		Phrases ends_with;
	};
	std::vector<Position> _positions;
	Phrases _top;

	struct Item
	{
		Phrase::ptr phrase;
		int from;
		int to;

		Item(Phrase::ptr phrase, int from, int to) : phrase(move(phrase)), from(from), to(to) { }
	};
	struct ErrorOrder
	{
		bool operator()(const Item& a, const Item& b)
		{
			return a.phrase->errors.length() > b.phrase->errors.length();
		}
	};
	std::priority_queue<Item, std::vector<Item>, ErrorOrder> _agenda;

	void _match(const Phrase::ptr& a, const Phrase::ptr& b, int from, int to);

	void _generate_result(int length, Phrases&& so_far, std::vector<Phrases>& result);
public:

	void push(Phrase::ptr p);
	void push(const Phrases& alternatives);

	std::vector<Phrases> run();
};
