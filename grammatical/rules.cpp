#include "phrase.h"
#include <cassert>
#include <unordered_map>

namespace args
{
	template <Rel R>
	bool rel(const Argument& arg) { return arg.rel == R; }
	static constexpr auto head = rel<Rel::spec>;
	static constexpr auto mod = rel<Rel::mod>;
	static constexpr auto comp = rel<Rel::comp>;
	static constexpr auto bicomp = rel<Rel::bicomp>;

	static auto sem = ranged::map([](const Argument& arg) { return arg.sem; });

	template <Rel R>
	auto matching(const Lexeme::ptr& s)
	{ 
		return [=](const Argument& arg) { return arg.rel == R && s->is(arg.sem); };
	}

	template <Rel R>
	auto matching(Mark m, const Lexeme::ptr& s)
	{
		return [=](const Argument& arg) { return arg.rel == R && arg.mark == m && s->is(arg.sem); };
	}
}
struct KeepHeadLexeme
{
	Lexeme::ptr operator()(Lexeme::ptr head) { return head; }
};

std::shared_ptr<LeftBranch> merge(const Mod& mod, char type, const Head& head, LeftRule l, RightRule r)
{
	auto result = std::make_shared<LeftBranch>(head->syn, head->sem, type, head, mod, l, r);
	result->args = head->args;
	return result;
}

std::shared_ptr<RightBranch> merge(const Head& head, char type, const Mod& mod, LeftRule l, RightRule r)
{
	auto result = std::make_shared<RightBranch>(head->syn, head->sem, type, head, mod, l, r);
	result->args = head->args;
	return result;
}

bool subject_verb_agreement(const Mod& mod, const Head& head)
{
	return !head->syn.hasAll(Tag::pres | Tag::fin) ||
		head->syn.hasAll(tags::sg3) == mod->syn.hasAll(tags::sg3);
}
bool subject_be_agreement(const Mod& mod, const Head& head)
{
	// being, been
	if (head->syn.has(Tag::part))
		return true;
	// are, were
	if (head->syn.hasAll(Tag::pl | Tag::second) && mod->syn.hasAny(Tag::pl | Tag::second))
		return true; 
	// am, is, was
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

		if (auto branch = std::dynamic_pointer_cast<const BinaryPhrase>(mod))
		{
			assert(branch->type == '+');
			if (const auto M = mark(branch->head->toString()); M && *M != Mark::None)
			{
				if (auto arg_match = result->args.extract(args::matching<Rel::mod>(*M, branch->mod->sem)); arg_match.empty())
					result->errors.emplace("preposition " + mod->toString() + " does not match " + head->toString());

			}
			else
				result->errors.emplace("preposition phrase " + head->toString() + " is not a valid mark");
		}
		else
			result->errors.emplace("preposition " + mod->toString() + " modifying " + head->toString() + " has no complement");

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

void check_verbal_object(const Head& head, const Mod& mod, const Phrase::mut_ptr& match)
{
	if (mod->syn.hasAny(Tag::fin | Tag::part) && mod->hasBranch(':'))
		match->errors.emplace("verbal object to " + head->toString() + " cannot have subject");
}

template <RightRule NextRight>
RuleOutput head_comp(const Head& head, const Mod& mod)
{
	auto result = NextRight(head, mod);
	if (mod->syn.hasAny(Tag::akk | Tag::fin | Tag::part | Tag::adn))
	{
		const auto match = merge(head, '+', mod, head->left_rule, NextRight);

		if (!mod->sem || !mod->sem->matchesAny(head->args.select(args::comp) | args::sem))
			match->errors.emplace("direct object " + mod->toString() + " does not match " + head->toString());

		check_verbal_object(head, mod, match);

		match->args.erase(args::comp);
		result.emplace_back(match);
	}
	return result;
}
template <RightRule NextRule>
RuleOutput verb_bicomp(const Head& head, const Mod& mod)
{
	auto result = NextRule(head, mod);

	if (mod->syn.hasAny(Tag::akk))
	{
		const auto match = merge(head, '*', mod, head->left_rule, NextRule);

		if (!mod->sem || !mod->sem->matchesAny(head->args.select(args::bicomp) | args::sem))
			match->errors.emplace("indirect object " + mod->toString() + " does not match " + head->toString());

		check_verbal_object(head, mod, match);

		match->args.erase(args::bicomp);
		result.emplace_back(match);
	}
	return result;
}

RuleOutput verb_adv(const Head& head, const Mod& mod)
{
	if (mod->syn.has(Tag::adv))
	{
		const auto match = merge(head, '<', mod, head->left_rule, head_prep);

		return { match };
	}
	return head_prep(head, mod);
}


RuleOutput be_lspec(const Mod& mod, const Head& head)
{
	if (mod->syn.has(Tag::nom))
	{
		auto match = merge(mod, ':', head, no_left, no_right);
		if (!subject_be_agreement(mod, head))
			match->errors.emplace("subject " + mod->toString() + "does not agree with " + head->toString());
		return { match };
	}
	return {};
}


template <Tag... Tense>
RuleOutput aux_comp(const Head& head, const Mod& mod)
{
	auto result = head_prep(head, mod);
	if (mod->syn.hasAny(Tag::akk))
	{
		result.emplace_back(merge(head, '+', mod, head->left_rule, head_prep));
	}
	else if (mod->syn.hasAny(Tag::fin | Tag::part))
	{
		auto match = merge(head, '+', mod, head->left_rule, head_prep);

		if (!mod->syn.hasAll((Tense | ...)))
			match->errors.emplace("tense of object " + mod->toString() + " does not agree with auxillary " + head->toString());

		check_verbal_object(head, mod, match);

		result.emplace_back(match);
	}

	return result;

}


RuleOutput be_rspec(const Head& head, const Mod& mod)
{
	auto result = aux_comp<Tag::part>(head, mod);

	if (mod->syn.has(Tag::nom))
	{
		auto match = merge(head, ':', mod, no_left, aux_comp<Tag::part>);
		if (!subject_be_agreement(mod, head))
			match->errors.emplace("subject " + mod->toString() + " does not agree with verb " + head->toString());
		result.emplace_back(match);
	}
	return result;
}

template <RightRule NextRule>
RuleOutput aux_rspec(const Head& head, const Mod& mod)
{
	auto result = NextRule(head, mod);

	if (mod->syn.has(Tag::nom))
	{
		auto match = merge(head, ':', mod, no_left, NextRule);
		if (!subject_verb_agreement(mod, head))
			match->errors.emplace("subject " + mod->toString() +" does not agree with verb " + head->toString());
		result.emplace_back(match);
	}
	return result;
}

Word::Word(Lexeme::ptr lexeme, Phrase::ptr morph) : Phrase{ 1, morph->syn, move(lexeme),{} }, _morph{ morph }
{
	static constexpr auto have_right = aux_rspec<aux_comp<Tag::part, Tag::past>>;
	static constexpr auto presf_aux_right = aux_rspec<aux_comp<Tag::fin, Tag::pres, Tag::pl>>;
	args = _morph->args;
	static const std::unordered_map<string, std::pair<LeftRule, RightRule>> special = 
	{
		{ "is",{ be_lspec, be_rspec } },
		{ "am",{ be_lspec, be_rspec } },
		{ "are", { be_lspec, be_rspec } },
		{ "have", { verb_spec, have_right }},
		{ "has", { verb_spec, have_right }},
		{ "had", { verb_spec, have_right }},
		{ "having", { no_left, have_right }},
		{ "do", { verb_spec, presf_aux_right }},
		{ "does", { verb_spec, presf_aux_right }},
		{ "did", { verb_spec, presf_aux_right }},
		{ "doing", { no_left, presf_aux_right }},
		{ "done", { no_left, presf_aux_right }}
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
			right_rule = verb_bicomp<head_comp<verb_adv>>;
		}
		else if (syn.has(Tag::adn))
		{
			left_rule = ad_adad;
		}
		else if (syn.has(Tag::prep))
		{
			left_rule = no_left;
			right_rule = head_comp<no_right>;
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
				+ (Tag::part | Tag::pres) };
		}
		if (morph->orth== "ed")
		{
			auto match = merge(head, '-', mod, no_left, noun_suffix)
				- (Tag::fin | tags::person | tags::number | tags::verb_regularity)
				+ (Tag::past | Tag::fin | Tag::part);
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

void Morpheme::_add_args(const Lexeme& s)
{
	for (auto&& a : s.args)
		args.emplace(a);
	for (auto&& ss : s.sem)
		_add_args(*ss);
}

void Morpheme::update(Tags new_syn, Lexeme::ptr new_sem)
{
	syn = new_syn;
	sem = move(new_sem);

	if (sem)
		_add_args(*sem);

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
