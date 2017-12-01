#include "phrase.h"

std::shared_ptr<BinaryPhrase> merge(Tags add, Tags agree, Phrase::ptr head, Branch branch, Phrase::ptr mod, Rule left_rule, Rule right_rule)
{
	return std::make_shared<BinaryPhrase>(
		add | head->tags.agreement(with(mod->tags), on(agree)), head, branch, mod, left_rule, right_rule);
}


RuleOutput noun_det(RuleInput head, RuleInput mod)
{
	if (mod->has(Tag::det))
	{
		const auto result = merge(tags::none, tags::number, head, Branch::left, mod, no_rule, no_rule);

		if (!result->tags.has(tags::number))
			result->errors.emplace("det-noun number disagrement");
		return result;
	}
	return nullptr;
}

RuleOutput noun_adjective(RuleInput head, RuleInput mod)
{
	if (mod->has(Tag::adn))
		return merge(tags::none, tags::none, head, Branch::left, mod, noun_adjective, no_rule);
	return noun_det(head, mod);
}

RuleOutput head_prep(RuleInput head, RuleInput mod)
{
	if (mod->has(Tag::prep))
	{
		const auto result = merge(tags::none, tags::none, head, Branch::right, mod, head->left_rule, head_prep);
		if (dynamic_cast<const Word*>(mod.get()))
			result->errors.emplace("preposition has no complement");
		return result;
	}
	return nullptr;
}

RuleOutput noun_rmod(RuleInput head, RuleInput mod)
{
	if (mod->has(Tag::verb))
	{
		const auto result = merge(tags::none, tags::none, head, Branch::right, mod, noun_adjective, no_rule);

		if (!mod->has(Tag::part))
			result->errors.emplace("verb right-modifying noun must be a participle");
		else if (dynamic_cast<const Word*>(mod.get()))
			result->errors.emplace("verb phrase must be complex to right-modify a noun");

		return result;
	}
	return head_prep(move(head), move(mod));
}

RuleOutput verb_spec(RuleInput head, RuleInput mod)
{
	if (mod->has(Tag::noun))
	{
		const auto result = merge(Tag::spec, tags::none, head, Branch::left, mod, no_rule, no_rule);

		if (head->has(Tag::part))
		{
			if (!head->has(Tag::past))
				result->errors.emplace("present participle can't take subject");
		}

		if ((head->has(Tag::sg) && mod->has(Tag::pl | Tag::first | Tag::second)) ||
			(head->has(Tag::pl) && mod->has(Tag::sg) && mod->has(Tag::third)))
			result->errors.emplace("noun-verb number/person disagreement");

		return result;
	}
	return nullptr;
}
RuleOutput verb_comp(RuleInput head, RuleInput mod)
{
	if (mod->has(Tag::noun))
		return merge(Tag::od, tags::none, head, Branch::right, mod, head->left_rule, head_prep);
	if (mod->has(Tag::verb))
	{
		const auto result = merge(Tag::od, tags::none, head, Branch::right, mod, head->left_rule, head_prep);

		if (!mod->has(Tag::part))
			result->errors.emplace("verb complemment must be a participle");
		else if (mod->has(Tag::past) && !head->has(Tag::aux_past))
			result->errors.emplace("non-past-auxillary verb cannot take past participle complement");
		else if (!mod->has(Tag::past) && !head->has(Tag::aux_pres))
			result->errors.emplace("non-present-auxillary verb cannot take present participle complement");

		return result;
	}
	return head_prep(move(head), move(mod));
}

Word::Word(string orth, Tags tags) : Phrase{ 1, tags,{} }, orth(move(orth))
{
	if (tags.has(Tag::noun))
	{
		left_rule = noun_adjective;
		right_rule = noun_rmod;
	}
	else if (tags.has(Tag::verb))
	{
		left_rule = verb_spec;
		right_rule = verb_comp;
	}
	else if (tags.has(Tag::prep))
	{
		left_rule = no_rule;
		right_rule = verb_comp;
	}
}
