// Доступ к таблице правил агента. Скопировано из tasks/1.3/skeleton.

#ifndef NANO_EDR_AGENT_RULES_H
#define NANO_EDR_AGENT_RULES_H

#include "rules.h"

namespace nano_edr {

// Таблица правил агента и её длина — ровно то, что принимает CheckRules.
const Rule* AgentRules();
size_t AgentRuleCount();

}  // namespace nano_edr

#endif  // NANO_EDR_AGENT_RULES_H
