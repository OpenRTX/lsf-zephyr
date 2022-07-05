#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "dict_search.h"
#include "dict.h"
#include "rule.h"

static float _dict_search(const char* const in_word, char* out, dict_inst_t inst, dictCallback_t cb, void *deccompress_data, dict_fq_e mode)
{
	dict_param_t dict_param;
	dict_param.callback = cb;
	dict_param.value = out;
	dict_param.value_len = 0;
	dict_param.key = in_word;
	dict_param.key_len = strlen(in_word) + 1;
	dict_param.fq_mode = mode;
	dict_param.score = 0;
	dict_err_e g_dict_ret = ls_dict_search(inst, &dict_param);
	if (deccompress_data) {
		free(deccompress_data);
	}
	if (g_dict_ret != CD_NO_ERR) {
		return 0;
	} else {
		return (mode == FUZZY_QUERY_DISABLE ? 1 : dict_param.score);
	}
}

int dict_search(const unsigned int in_size, const char* const in_word, char* out, dict_inst_t inst, dictCallback_t cb, void* deccompress_data)
{
	if (_dict_search(in_word, out, inst, cb, deccompress_data, FUZZY_QUERY_DISABLE) != 1) {
		//原词查询不到，通过匹配规则找相似的单词
		ExecRuleResult *rule_result = execAllRules(in_word);
		if (rule_result != NULL) {
			float temp_score = 0;

			for (int i = 0; i < rule_result->count; i++) {
				//通过相似的词语查词
				float score = _dict_search(rule_result->result[i], out, inst, cb, deccompress_data, FUZZY_QUERY_DISABLE);
				if (score > 0) {
					temp_score = 1;
					break;
				}
			}
			
			freeExecRuleResult(rule_result);
			if (temp_score <= 0 ) return 0;
			return 1;
		} else {
			return 0;
		}
	} else {//完全匹配
		return 1;
	}
}
