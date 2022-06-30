#ifndef __RULE__
#define __RULE__

#include <stdio.h>

typedef struct {
	char **result;
	int count;
} ExecRuleResult;

ExecRuleResult *execAllRules(const char *strIn);
void freeExecRuleResult(ExecRuleResult *ptr);

#endif
