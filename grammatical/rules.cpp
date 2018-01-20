#include "phrase.h"
#include <unordered_map>

struct KeepHeadLexeme
{
	Lexeme::ptr operator()(Lexeme::ptr head) { return head; }
};

std::shared_ptr<LeftBranch> merge(const Mod& mod, char type, const Head& head, LeftRule l, RightRule r)
{
	return std::make_shared<LeftBranch>(head->syn, head->sem, type, head, mod, l, r);
}

std::shared_ptr<RightBranch> merge(const Head& head, char type, const Mod& mod, LeftRule l, RightRule r)
{
	return std::make_shared<RightBranch>(head->syn, head->sem, type, head, mod, l, r);
}

bool subject_verb_agreement(const Mod& mod, const Head& head)
{
	return !head->syn.hasAll(Tag::pres | Tag::fin) ||
		head->syn.hasAll(tags::sg3) == mod->syn.hasAll(tags::sg3);
}
bool subject_be_agreement(const Mod& mod, const Head& head)
{
	if (head->syn.has(Tag::part))
		return true;
	if (head->syn.hasAll(Tag::pl | Tag::second) && !mod->syn.hasAny(Tag::pl | Tag::second))
		return false;
	if (head->syn.has(Tag::sg) && !mod->syn.has(Tag::sg))
		return false;
	return head->agreesOn(Tag::first | Tag::second | Tag::third).with(*mod);
}

template <class A, class B>
decltype(auto) append(A&& a, B&& b)
{
	a.insert(a.end(), b.begin(), b.end());
	return std::forward<A>(a);
}



RuleOutput noun_det(const Mod& mod, const Head& head)
{
	if (mod->syn.has(Tag::gen))
	{
		const auto result = merge(mod, ':', head, no_left, no_right);

		if (!head->agreesOn(Tag::sg | Tag::pl | Tag::uc).with(*mod))
			result->errors.emplace("det and noun not compatible");
		return { result };
	}
	return {};
}

RuleOutput ad_adad(const Mod& mod, const Head& head)
{
	if (mod->syn.has(Tag::adad))
		return { merge(mod, '>', head, no_left, no_right) };
	return {};
}

RuleOutput noun_adjective(const Mod& mod, const Head& head)
{
	if (mod->syn.has(Tag::adn))
		return { merge(mod, '>', head, noun_adjective, no_right) };
	return noun_det(mod, head);
}

RuleOutput head_prep(const Head& head, const Mod& mod)
{
	if (mod->syn.has(Tag::prep))
	{
		const auto result = merge(head, '<', mod, head->left_rule, head_prep);
		if (!mod->hasBranch('+'))
			result->errors.emplace("preposition has no complement");
		return { result };
	}
	return {};
}

RuleOutput noun_rmod(const Head& head, const Mod& mod)
{
	if (mod->syn.has(Tag::part))
	{
		const auto result = merge(head, '<', mod, noun_adjective, no_right);

		if (!mod->syn.has(Tag::part))
			result->errors.emplace("verb right-modifying noun must be a participle");
		else
		{
			if (mod->syn.has(Tag::past) && (mod->hasBranch('+') || mod->hasBranch('*')))
				result->errors.emplace("past participle modifying noun can't have an object");
			if (mod->syn.has(Tag::pres) && mod->hasBranch(':'))
				result->errors.emplace("present participle modifying noun can't have subject");
			if (dynamic_cast<const Word*>(mod.get()))
				result->errors.emplace("verb phrase must be complex to right-modify a noun");
		}

		return { result };
	}
	return head_prep(head, mod);
}

RuleOutput verb_spec(const Mod& mod, const Head& head)
{
	if (mod->syn.has(Tag::nom))
	{
		const auto result = merge(mod, ':', head, no_left, no_right);

		if (!head->syn.has(Tag::fin))
			result->errors.emplace("verb participle cannot take a subject");
		if (!subject_verb_agreement(mod, head))
			result->errors.emplace("verb-subject disagreement");

		return { result };
	}
	return {};
}
RuleOutput head_comp(const Head& head, const Mod& mod, RightRule next_right)
{
	auto result = next_right(head, mod);
	if (mod->syn.hasAny(Tag::akk | Tag::fin | Tag::part | Tag::adn))
	{
		const auto match = merge(head, '+', mod, head->left_rule, next_right);


		if (mod->syn.hasAny(Tag::fin | Tag::part) && mod->hasBranch(':'))
			match->errors.emplace("verbal object cannot have a subject");

		result.emplace_back(match);
	}
	return result;
}
RuleOutput prep_comp(const Head& head, const Mod& mod) { return head_comp(head, mod, no_right); }

RuleOutput verb_adv(const Head& head, const Mod& mod)
{
	if (mod->syn.has(Tag::adv))
	{
		const auto match = merge(head, '<', mod, head->left_rule, head_prep);

		return { match };
	}
	return head_prep(head, mod);
}

RuleOutput verb_comp(const Head& head, const Mod& mod)
{
	return head_comp(head, mod, verb_adv);
}
RuleOutput verb_bicomp(const Head& head, const Mod& mod)
{
	auto result = verb_comp(head, mod);
	if (mod->syn.has(Tag::akk))
	{
		const auto match = merge(head, '*', mod, head->left_rule, verb_comp);

		result.emplace_back(match);
	}
	return result;
}
RuleOutput verb_rspec(const Head& head, const Mod& mod)
{
	auto result = verb_bicomp(head, mod);
	if (mod->syn.has(Tag::nom))
	{
		const auto match = merge(head, ':', mod, no_left, verb_bicomp);
		
		if (!subject_verb_agreement(mod, head))
			match->errors.emplace("noun-verb number/person disagreement");

		result.emplace_back(match);
	}
	return result;
}

RuleOutput be_lspec(const Mod& mod, const Head& head)
{
	if (mod->syn.has(Tag::nom))
	{
		auto match = merge(mod, ':', head, no_left, no_right);
		if (!subject_be_agreement(mod, head))
			match->errors.emplace("subject does not agree with verb");
		return { match };
	}
	return {};
}
RuleOutput be_comp(const Head& head, const Mod& mod)
{
	auto result = head_prep(head, mod);
	if (mod->syn.hasAny(Tag::akk | Tag::adn))
	{
		result.emplace_back(merge(head, '+', mod, head->left_rule, head_prep));
	}
	else if (mod->syn.hasAny(Tag::fin | Tag::part))
	{
		auto match = merge(head, '+', mod, head->left_rule, head_prep);

		if (!mod->syn.hasAll(Tag::fin | Tag::pres | Tag::pl))
			match->errors.emplace("verb object of 'to be' must be dictionary form");

		result.emplace_back(match);
	}

	return result;
}


RuleOutput be_rspec(const Head& head, const Mod& mod)
{
	auto result = be_comp(head, mod);
	if (mod->syn.has(Tag::nom))
	{
		auto match = merge(head, ':', mod, no_left, be_comp);
		if (!subject_be_agreement(mod, head))
			match->errors.emplace("subject does not agree with verb");
		result.emplace_back(match);
	}
	return result;
}


RuleOutput have_comp(const Head& head, const Mod& mod)
{
	auto result = head_prep(head, mod);
	if (mod->syn.hasAny(Tag::akk))
	{
		result.emplace_back(merge(head, '+', mod, head->left_rule, head_prep));
	}
	else if (mod->syn.hasAny(Tag::fin | Tag::part))
	{
		auto match = merge(head, '+', mod, head->left_rule, head_prep);

		if (!mod->syn.hasAll(Tag::past | Tag::part))
			match->errors.emplace("verb object of 'to have' must be past participle");

		result.emplace_back(match);
	}

	return result;
}


RuleOutput have_rspec(const Head& head, const Mod& mod)
{
	auto result = have_comp(head, mod);
	if (mod->syn.has(Tag::nom))
	{
		auto match = merge(head, ':', mod, no_left, have_comp);
		if (!subject_verb_agreement(mod, head))
			match->errors.emplace("subject does not agree with verb");
		result.emplace_back(match);
	}
	return result;
}

Word::Word(Lexeme::ptr lexeme, Phrase::ptr morph) : Phrase{ 1, morph->syn, move(lexeme),{} }, _morph{ morph }
{
	static const std::unordered_map<string, std::pair<LeftRule, RightRule>> special = 
	{
		{ "is",{ be_lspec, be_rspec} },
		{ "have", { verb_spec, have_rspec }},
		{ "has", { verb_spec, have_rspec }},
		{ "had", { verb_spec, have_rspec }},
		{ "having", { no_left, have_rspec }}
	};
	if (auto m = std::dynamic_pointer_cast<const Morpheme>(_morph))
		if (auto found = special.find(m->orth); found != special.end())
		{
			left_rule = found->second.first;
			right_rule = found->second.second;
		}
	if (left_rule == no_left && right_rule == no_right) // nothing special was found
	{
		if (syn.hasAny(Tag::nom | Tag::akk))
		{
			left_rule = noun_adjective;
			right_rule = noun_rmod;
		}
		else if (syn.hasAny(Tag::fin | Tag::part))
		{
			left_rule = syn.has(Tag::fin) ? verb_spec : no_left;
			right_rule = syn.has(Tag::free) ? verb_rspec : verb_bicomp;
		}
		else if (syn.has(Tag::adn))
		{
			left_rule = ad_adad;
		}
		else if (syn.has(Tag::prep))
		{
			left_rule = no_left;
			right_rule = prep_comp;
		}
	}
}

std::shared_ptr<Phrase> operator+(std::shared_ptr<Phrase> p, Tags tags)
{
	p->syn.insert(tags);
	return p;
}
std::shared_ptr<Phrase> operator-(std::shared_ptr<Phrase> p, Tags tags)
{
	p->syn.remove(tags);
	return p;
}

RuleOutput noun_suffix(const Head& head, const Mod& mod)
{
	if (auto morph = std::dynamic_pointer_cast<const Morpheme>(mod); 
		morph && mod->syn.has(Tag::suffix))
	{
		if (morph->orth == "s")
		{
			return { merge(head, '-', mod, no_left, no_right) - (Tag::sg | Tag::rc) + Tag::pl };
		}
	}
	return {};
}

RuleOutput verb_suffix(const Head& head, const Mod& mod)
{
	if (auto morph = std::dynamic_pointer_cast<const Morpheme>(mod); 
		morph && mod->syn.has(Tag::suffix))
	{
		if (morph->orth == "ing")
		{
			return { merge(head, '-', mod, no_left, noun_suffix)
				- (Tag::fin | tags::person | tags::number | tags::verb_regularity)
				+ (Tag::part | Tag::pres | Tag::nom | Tag::akk | tags::sg3 | Tag::rc | Tag::adn) };
		}
		if (morph->orth== "ed")
		{
			auto match = merge(head, '-', mod, no_left, noun_suffix)
				- (Tag::fin | tags::person | tags::number | tags::verb_regularity)
				+ (Tag::past | Tag::fin | Tag::part | Tag::nom | Tag::akk | tags::sg3 | Tag::rc | Tag::adn);
			if (!head->syn.has(Tag::rpast))
				match->syn.remove(Tag::fin);
			if (!head->syn.has(Tag::rpart))
				match->syn.remove(Tag::part);
			if (!head->syn.hasAny(Tag::rpart | Tag::rpast))
				match->errors.emplace("verb does not have a regular past tense");
			return { match };
		}
		if (morph->orth == "er" || morph->orth == "ee")
		{
			return { merge(head, '-', mod, no_left, noun_suffix)
				- (Tag::fin | tags::person | tags::number | tags::verb_regularity)
				+ (Tag::nom | Tag::akk | tags::sg3 | Tag::rc) };
		}
	}
	return {};
}

void Morpheme::update(Tags new_syn, Lexeme::ptr new_sem)
{
	syn = new_syn;
	sem = move(new_sem);

	if (syn.has(Tag::rc))
	{
		right_rule = noun_suffix;
	}
	if (syn.hasAll(Tag::fin | Tag::pres | Tag::pl))
	{
		right_rule = verb_suffix;
	}
	else if (syn.hasAny(Tag::verbe | Tag::verby))
	{
		right_rule = verb_suffix;
	}
}
