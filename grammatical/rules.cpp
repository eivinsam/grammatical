#include "phrase.h"

std::shared_ptr<LeftBranch> merge(Lexeme::ptr lex, const Mod& mod, const Head& head, LeftRule l, RightRule r)
{
	return std::make_shared<LeftBranch>(move(lex), head, mod, l, r);
}

std::shared_ptr<RightBranch> merge(Lexeme::ptr lex, const Head& head, const Mod& mod, LeftRule l, RightRule r)
{
	return std::make_shared<RightBranch>(move(lex), head, mod, l, r);
}

RuleOutput noun_det(const Mod& mod, const Head& head)
{
	if (mod->is(tag::gen))
	{
		const auto result = merge(head->lex, mod, head, no_left, no_right);

		if (!mod->lex->is(head->lex->spec))
			result->errors.emplace("det and noun not compatible");
		return { result };
	}
	return {};
}

RuleOutput ad_adad(const Mod& mod, const Head& head)
{
	if (mod->is(tag::adad))
		return { merge(head->lex, mod, head, no_left, no_right) };
	return {};
}

RuleOutput noun_adjective(const Mod& mod, const Head& head)
{
	if (mod->is(tag::adn))
		return { merge(head->lex, mod, head, noun_adjective, no_right) };
	return noun_det(mod, head);
}

//RuleOutput head_prep(RuleInput head, RuleInput mod)
//{
//	if (mod->has(Tag::prep))
//	{
//		const auto result = merge(tags::none, tags::none, head, Branch::right, mod, head->left_rule, head_prep);
//		if (dynamic_cast<const Word*>(mod.get()))
//			result->errors.emplace("preposition has no complement");
//		return result;
//	}
//	return nullptr;
//}

RuleOutput noun_rmod(const Head& head, const Mod& mod)
{
	if (mod->is(tag::part))
	{
		const auto result = merge(head->lex, head, mod, noun_adjective, no_right);

		if (!mod->is(tag::part))
			result->errors.emplace("verb right-modifying noun must be a participle");
		else if (dynamic_cast<const Word*>(mod.get()))
			result->errors.emplace("verb phrase must be complex to right-modify a noun");

		return { result };
	}
	return {};//head_prep(move(head), move(mod));
}

RuleOutput verb_spec(const Mod& mod, const Head& head)
{
	if (mod->is(tag::nom))
	{
		const auto result = merge(head->lex, mod, head, no_left, no_right);

		if (!mod->is(head->lex->spec))
			result->errors.emplace("noun-verb number/person disagreement");

		return { result };
	}
	return {};
}
RuleOutput verb_comp(const Head& head, const Mod& mod)
{
	if (mod->is(tag::akk))
		return { merge(head->lex, head, mod, head->left_rule, no_right) };//head_prep);
	//if (mod->has(Tag::verb))
	//{
	//	const auto result = merge(Tag::od, tags::none, head, Branch::right, mod, head->left_rule, head_prep);
	//
	//	if (!mod->has(Tag::part))
	//		result->errors.emplace("verb complemment must be a participle");
	//	else if (mod->has(Tag::past) && !head->has(Tag::aux_past))
	//		result->errors.emplace("non-past-auxillary verb cannot take past participle complement");
	//	else if (!mod->has(Tag::past) && !head->has(Tag::aux_pres))
	//		result->errors.emplace("non-present-auxillary verb cannot take present participle complement");
	//
	//	return result;
	//}
	return {};// head_prep(move(head), move(mod));
}
RuleOutput verb_rspec(const Head& head, const Mod& mod)
{
	auto result = verb_comp(head, mod);
	if (mod->is(tag::nom))
	{
		const auto match = merge(head->lex, head, mod, no_left, verb_comp);
		
		if (!mod->is(head->lex->spec))
			match->errors.emplace("noun-verb number/person disagreement");

		result.emplace_back(match);
	}
	return result;
}


Word::Word(Lexeme::ptr lexeme) : Phrase{ 1, move(lexeme),{} }
{
	if (lex->is("nom") || lex->is("akk"))
	{
		left_rule = noun_adjective;
		right_rule = noun_rmod;
	}
	else if (lex->is("verb"))
	{
		left_rule = lex->is("fin") ? verb_spec : no_left;
		right_rule = lex->is("free") ? verb_rspec : verb_comp;
	}
	else if (lex->is("ad"))
	{
		left_rule = ad_adad;
	}
	//else if (tags.has(Tag::prep))
	//{
	//	left_rule = no_rule;
	//	right_rule = verb_comp;
	//}
}
