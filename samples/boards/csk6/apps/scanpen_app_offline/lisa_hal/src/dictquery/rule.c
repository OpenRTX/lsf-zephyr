#include "rule.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "dict_search.h"

typedef struct {
	uint8_t type;		/* 1:后置匹配**ing此类  2:后置完全匹配  3:前置完全匹配 */
	const char *match;	/* 匹配的文本 */
	uint8_t match_len;	/* 匹配的文本的长度 */
	const char *remove;	/* 要删除的文本，为优化效率实际未用到 */
	uint8_t remove_len;	/* 要删除的文本的长度 */
	const char *append;	/* 要在尾部追加的文本 */
	uint8_t append_len;	/* 要在尾部追加的文本的长度 */
} Word_Rule_Desc;

/* 后置规则 */
const Word_Rule_Desc Suffix_Rules[] = {
/* ing结尾 */
	{1, "ing", 3, "?ing", 4, 0, 0},  /* 后置匹配**ing，删除*ing */
	{2, "ing", 3, "ing", 3, 0, 0},	 /* 后置匹配ing，删除ing */
	{2, "ing", 3, "ing", 3, "e", 1}, /* 后置匹配ing，删除ing，尾部加e */
	{2, "ing", 3, "ing", 3, "y", 1},
	{1, "ening", 5, "?ening", 6, 0, 0},
	{2, "ening", 5, "ening", 5, 0, 0},
	{2, "ening", 5, "ening", 5, "e", 1},
	{2, "ering", 5, "ering", 5, 0, 0},
	{2, "ering", 5, "ering", 5, "e", 1},

/* ed结尾 */
	{1, "ed", 2, "?ed", 3, 0, 0},
	{2, "ied", 3, "ied", 3, "y", 1},
	{2, "ed", 2, "ed", 2, 0, 0},
	{2, "ed", 2, "d", 1, 0, 0},
	{2, "ized", 4, "ized", 4, 0, 0},
	{2, "ised", 4, "ised", 4, 0, 0},
	{2, "ized", 4, "ized", 4, "e", 1},
	{2, "ised", 4, "ised", 4, "e", 1},
	{2, "cked", 4, "ked", 3, 0, 0},

/* s结尾 */
	{2, "s", 1, "s", 1, 0, 0},
	{2, "ses", 3, "es", 2, 0, 0},
	{2, "xes", 3, "es", 2, 0, 0},
	{2, "ches", 4, "es", 2, 0, 0},
	{2, "shes", 4, "es", 2, 0, 0},
	{2, "oes", 3, "es", 2, 0, 0},
	{2, "ies", 3, "ies", 3, "y", 1},
	{1, "ess", 3, "?ess", 4, 0, 0},
	{2, "ess", 3, "ess", 3, 0, 0},
	{2, "less", 4, "less", 4, 0, 0},
	{2, "iless", 5, "iless", 5, "y", 1},
	{2, "ness", 4, "ness", 4, 0, 0},
	{1, "ableness", 8, "?ableness", 9, 0, 0},
	{2, "ableness", 8, "ableness", 8, 0, 0},
	{2, "ableness", 8, "ableness", 8, "e", 1},
	{2, "iableness", 9, "iableness", 9, "y", 1},	
	{2, "iness", 5, "iness", 5, "y", 1},
	{2, "iness", 5, "iness", 5, "e", 1},
	{2, "iness", 5, "iness", 5, 0, 0},
	{2, "liness", 6, "liness", 6, 0, 0},
	{2, "lessness", 8, "lessness", 8, 0, 0},
	{2, "ousness", 7, "ousness", 7, 0, 0},
	{2, "ousness", 7, "ousness", 7, "e", 1},
	{2, "iousness", 8, "iousness", 8, 0, 0},
	{2, "iousness", 8, "iousness", 8, "y", 1},
	{2, "iousness", 8, "iousness", 8, "e", 1},
	{2, "wards", 5, "wards", 5, 0, 0},
	{2, "ous", 3, "ous", 3, 0, 0},
	{2, "ous", 3, "ous", 3, "e", 1},
	{2, "eous", 4, "eous", 4, 0, 0},
	{2, "eous", 4, "eous", 4, "e", 1},
	{2, "ious", 4, "ious", 4, "y", 1},
	{2, "ious", 4, "ious", 4, "e", 1},
	{1, "ious", 4, "?ious", 5, 0, 0},
	{2, "ious", 4, "ious", 4, 0, 0},

/* r结尾 */	
	{1, "er", 2, "?er", 3, 0, 0},
	{2, "er", 2, "er", 2, 0, 0},
	{2, "er", 2, "r", 1, 0, 0},
	{2, "eer", 3, "eer", 3, 0, 0},
	{2, "ier", 3, "ier", 3, 0, 0},
	{2, "ier", 3, "ier", 3, "y", 1},
	{2, "ier", 3, "ier", 3, "e", 1}, 
	{2, "izer", 4, "izer", 4, 0, 0},
	{2, "izer", 4, "izer", 4, "e", 1},
	{2, "ioner", 5, "ioner", 5, 0, 0},
	{2, "ioner", 5, "ioner", 5, "y", 1},
	{1, "or", 2, "?or", 3, 0, 0},
	{2, "or", 2, "or", 2, 0, 0},
	{2, "or", 2, "or", 2, "e", 1},
	{2, "maker", 5, "maker", 5, 0, 0},

/* t结尾 */	
	{2, "iest", 4, "iest", 4, "y", 1},
	{2, "est", 3, "est", 3, 0, 0},
	{2, "est", 3, "st", 2, 0, 0},
	{1, "ist", 3, "?ist", 4, 0, 0},
	{2, "ist", 3, "ist", 3, 0, 0},
	{2, "ist", 3, "ist", 3, "e", 1},
	{2, "ist", 3, "ist", 3, "y", 1},
	{2, "ant", 3, "ant", 3, 0, 0},
	{2, "ant", 3, "ant", 3, "e", 1},
	{2, "ent", 3, "ent", 3, 0, 0},
	{2, "ent", 3, "ent", 3, "e", 1},
	{2, "iment", 5, "iment", 5, "y", 1},
	{2, "ment", 4, "ment", 4, 0, 0},
	{2, "ment", 4, "ment", 4, "e", 1},
	{2, "most", 4, "most", 4, 0, 0},

/* y结尾 */
	{1, "y", 1, "?y", 2, 0, 0},
	{2, "y", 1, "y", 1, 0, 0},
	{2, "y", 1, "y", 1, "e", 1},
	{2, "ly", 2, "ly", 2, 0, 0},
	{2, "ly", 2, "y", 1, "e", 1},
	{2, "ly", 2, "y", 1, 0, 0},
	{2, "ably", 4, "ably", 4, 0, 0},
	{2, "ably", 4, "ably", 4, "e", 1},
	{1, "ably", 4, "?ably", 5, 0, 0},
	{2, "iably", 5, "iably", 5, "y", 1},
	{2, "ibly", 4, "ibly", 4, "e", 1},
	{2, "ibly", 4, "ibly", 4, 0, 0},
	{1, "edly", 4, "?edly", 5, 0, 0},
	{2, "edly", 4, "edly", 4, 0, 0},
	{2, "edly", 4, "edly", 4, "e", 1},
	{2, "iedly", 5, "iedly", 5, "y", 1},
	{2, "wardly", 6, "wardly", 6, 0, 0},
	{2, "ately", 5, "ately", 5, 0, 0},
	{2, "ately", 5, "ately", 5, "e", 1},
	{2, "ively", 5, "ively", 5, 0, 0},
	{2, "ively", 5, "ively", 5, "e", 1},
	{2, "ively", 5, "ively", 5, "ion", 3},
	{2, "atively", 7, "atively", 7, 0, 0},
	{2, "atively", 7, "atively", 7, "e", 1},
	{2, "atively", 7, "atively", 7, "y", 1},
	{2, "somely", 6, "somely", 6, 0, 0},
	{1, "ingly", 5, "?ingly", 6, "ly", 2},
	{1, "ingly", 5, "?ingly", 6, 0, 0},
	{2, "ingly", 5, "ingly", 5, 0, 0},
	{2, "ingly", 5, "ingly", 5, "e", 1},	
	{2, "eningly", 7, "eningly", 7, 0, 0},
	{2, "thly", 4, "thly", 4, 0, 0},
	{2, "ishly", 5, "ishly", 5, 0, 0},
	{2, "ishly", 5, "ishly", 5, "e", 1},
	{2, "ily", 3, "ily", 3, "y", 1},
	{2, "ily", 3, "ily", 3, "e", 1},
	{2, "ily", 3, "ily", 3, 0, 0},
	{1, "ily", 3, "?ily", 4, 0, 0},
	{2, "ally", 4, "ally", 4, 0, 0},
	{2, "ally", 4, "ally", 4, "e", 1},
	{2, "ially", 5, "ially", 5, 0, 0},
	{2, "ially", 5, "ially", 5, "y", 1},
	{2, "ually", 5, "ually", 5, 0, 0},
	{2, "ically", 6, "ically", 6, "y", 1},
	{2, "ically", 6, "ically", 6, 0, 0},
	{2, "fully", 5, "fully", 5, 0, 0},
	{2, "ifully", 6, "ifully", 6, "y", 1},
	{2, "abilly", 6, "abilly", 6, 0, 0},
	{1, "erly", 4, "?erly", 5, 0, 0},
	{2, "erly", 4, "erly", 4, 0, 0},
	{2, "erly", 4, "erly", 4, "e", 1},
	{2, "ierly", 5, "ierly", 5, "y", 1},
	{2, "ously", 5, "ously", 5, 0, 0},
	{2, "ously", 5, "ously", 5, "e", 1},
	{2, "iously", 6, "iously", 6, "y", 1},
	{2, "iously", 6, "iously", 6, "e", 1},
	{2, "iously", 6, "iously", 6, 0, 0},
	{1, "iously", 6, "?iously", 7, 0, 0},
	{2, "eously", 6, "eously", 6, 0, 0},
	{2, "eously", 6, "eously", 6, "e", 1},
	{2, "aneously", 8, "aneously", 8, 0, 0},
	{2, "lessly", 6, "lessly", 6, 0, 0},
	{2, "ilessly", 7, "ilessly", 7, "y", 1},
	{2, "antly", 5, "antly", 5, "ate", 3},
	{2, "antly", 5, "antly", 5, 0, 0},
	{2, "antly", 5, "antly", 5, "e", 1},
	{2, "ently", 5, "ently", 5, 0, 0},
	{2, "ently", 5, "ently", 5, "e", 1},
	{2, "fy", 2, "fy", 2, 0, 0},
	{2, "fy", 2, "y", 1, "e", 1},
	{2, "ify", 3, "ify", 3, "y", 1},
	{2, "ify", 3, "ify", 3, 0, 0},
	{2, "ify", 3, "ify", 3, "e", 1},
	{2, "ty", 2, "ty", 2, 0, 0},
	{2, "ty", 2, "y", 1, "e", 1},
	{2, "ity", 3, "ity", 3, "y", 1},
	{2, "ity", 3, "ity", 3, 0, 0},
	{2, "ity", 3, "ity", 3, "e", 1},
	{2, "ivity", 5, "ivity", 5, 0, 0},
	{2, "ality", 5, "ality", 5, 0, 0},
	{2, "ality", 5, "ality", 5, "e", 1},
	{2, "ablity", 6, "ablity", 6, "able", 4},
	{2, "ablity", 6, "ablity", 6, 0, 0},
	{2, "ablity", 6, "ablity", 6, "e", 1},
	{2, "ility", 5, "ility", 5, "le", 2},					
	{2, "ry", 2, "ry", 2, 0, 0},
	{2, "ary", 3, "ary", 3, 0, 0},
	{2, "ary", 3, "ary", 3, "e", 1},
	{2, "cy", 2, "cy", 2, "t", 1},
	{2, "ency", 4, "ency", 4, 0, 0},
	{2, "ency", 4, "ency", 4, "e", 1},
	{2, "ancy", 4, "ancy", 4, 0, 0},
	{2, "ogy", 3, "ogy", 3, 0, 0},	
	{2, "ology", 5, "ology", 5, 0, 0},
	{2, "ology", 5, "ology", 5, "e", 1},
	{2, "boy", 3, "boy", 3, 0, 0},
	{2, "way", 3, "way", 3, 0, 0},
	{2, "way", 3, "way", 3, "e", 1},
	{2, "day", 3, "day", 3, 0, 0},
	{2, "day", 3, "day", 3, "e", 1},
	{2, "play", 4, "play", 4, 0, 0},
	{2, "play", 4, "play", 4, "e", 1},
	{2, "away", 4, "away", 4, 0, 0},
	{2, "away", 4, "away", 4, "e", 1},
	{2, "aday", 4, "aday", 4, 0, 0},
	{2, "aday", 4, "aday", 4, "e", 1},

/* n结尾 */
	{2, "ian", 3, "ian", 3, 0, 0},
	{2, "ian", 3, "ian", 3, "e", 1},
	{2, "ian", 3, "ian", 3, "y", 1},
	{2, "en", 2, "en", 2, 0, 0},
	{2, "ion", 3, "ion", 3, 0, 0},
	{2, "ion", 3, "ion", 3, "e", 1},
	{1, "ation", 5, "?ation", 6, 0, 0},
	{2, "ation", 5, "ation", 5, 0, 0},
	{2, "ation", 5, "ation", 5, "e", 1},
	{2, "ization", 7, "ization", 7, 0, 0},
	{2, "ization", 7, "ization", 7, "y", 1},
	{2, "isation", 7, "isation", 7, 0, 0},
	{2, "isation", 7, "isation", 7, "y", 1},
	{2, "ivation", 7, "ivation", 7, 0, 0},
	{2, "ition", 5, "ition", 5, 0, 0},
	{2, "ition", 5, "ition", 5, "e", 1},
	{2, "men", 3, "men", 3, 0, 0},
	{2, "man", 3, "man", 3, 0, 0},
	{2, "smen", 4, "smen", 4, 0, 0},
	{2, "sman", 4, "sman", 4, 0, 0},
	{2, "women", 5, "women", 5, 0, 0},
	{2, "woman", 5, "woman", 5, 0, 0},
	{2, "swomen", 6, "swomen", 6, 0, 0},
	{2, "swoman", 6, "swoman", 6, 0, 0},

/* m结尾 */
	{2, "ism", 3, "ism", 3, 0, 0},
	{2, "ism", 3, "ism", 3, "e", 1},	
	{2, "dom", 3, "dom", 3, 0, 0},

/* e结尾 */
	{2, "ance", 4, "ance", 4, "ate", 3},
	{1, "ance", 4, "?ance", 5, 0, 0},
	{2, "iance", 5, "iance", 5, "y", 1},
	{2, "ance", 4, "ance", 4, 0, 0},
	{2, "ance", 4, "ance", 4, "e", 1},
	{2, "ence", 4, "ence", 4, 0, 0},
	{2, "ence", 4, "ence", 4, "e", 1},
	{2, "ude", 3, "ude", 3, 0, 0},
	{2, "ee", 2, "ee", 2, 0, 0},
	{2, "ee", 2, "ee", 2, "e", 1},
	{1, "age", 3, "?age", 4, 0, 0},
	{2, "age", 3, "age", 3, 0, 0},
	{2, "age", 3, "age", 3, "e", 1},
	{2, "ie", 2, "ie", 2, 0, 0},
	{2, "ie", 2, "ie", 2, "y", 1},
	{1, "able", 4, "?able", 5, 0, 0},
	{2, "able", 4, "able", 4, 0, 0},
	{2, "able", 4, "able", 4, "e", 1},
	{2, "iable", 5, "iable", 5, "y", 1},
	{2, "ible", 4, "ible", 4, 0, 0},
	{2, "ible", 4, "ible", 4, "y", 1},
	{2, "ible", 4, "ible", 4, "e", 1},
	{2, "ate", 3, "ate", 3, 0, 0},
	{2, "ate", 3, "ate", 3, "e", 1},
	{2, "titude", 6, "titude", 6, 0, 0},
	{2, "ive", 3, "ive", 3, 0, 0},
	{2, "ive", 3, "ive", 3, "e", 1},
	{2, "ive", 3, "ive", 3, "ion", 3},
	{2, "tive", 4, "tive", 4, 0, 0},
	{2, "tive", 4, "tive", 4, "e", 1},
	{2, "ure", 3, "ure", 3, 0, 0},
	{2, "ure", 3, "ure", 3, "e", 1},
	{2, "ce", 2, "ce", 2, "t", 1},
	{2, "ise", 3, "ise", 3, 0, 0},
	{2, "ise", 3, "ise", 3, "y", 1},
	{2, "ise", 3, "ise", 3, "e", 1},
	{2, "wise", 4, "wise", 4, 0, 0},
	{2, "ize", 3, "ize", 3, 0, 0},
	{2, "ize", 3, "ize", 3, "y", 1},
	{2, "ize", 3, "ize", 3, "e", 1},

/* h结尾 */
	{2, "th", 2, "th", 2, 0, 0},
	{2, "th", 2, "th", 2, "t", 1},
	{2, "ieth", 4, "ieth", 4, "y", 1},
	{2, "ish", 3, "ish", 3, 0, 0},
	{2, "ish", 3, "ish", 3, "e", 1},

/* l结尾 */
	{2, "al", 2, "al", 2, 0, 0},
	{2, "al", 2, "al", 2, "e", 1},
	{2, "ial", 3, "ial", 3, "y", 1},
	{2, "ial", 3, "ial", 3, "e", 1},
	{2, "ial", 3, "ial", 3, 0, 0},
	{2, "tial", 4, "tial", 4, "ce", 2},
	{2, "ical", 4, "ical", 4, 0, 0},
	{2, "ical", 4, "ical", 4, "e", 1},
	{2, "ful", 3, "ful", 3, 0, 0},
	{2, "iful", 4, "iful", 4, "y", 1},

/* 其他*/
	{2, "ia", 2, "ia", 2, "y", 1},
	{2, "ic", 2, "ic", 2, 0, 0},
	{2, "ic", 2, "ic", 2, "e", 1},
	{2, "ward", 4, "ward", 4, 0, 0},
	{2, "ship", 4, "ship", 4, 0, 0},
	{2, "hood", 4, "hood", 4, 0, 0},
	{2, "lihood", 6, "lihood", 6, 0, 0},
	{2, "some", 4, "some", 4, 0, 0},	
	{2, "isome", 5, "isome", 5, "y", 1},
	{2, "like", 4, "like", 4, 0, 0},
};

/* 前置规则 */
const Word_Rule_Desc Prefix_Rules[] = {
	{3, "un", 2, "un", 2, 0, 0},	/* 前置匹配un，删除un */
	{3, "im", 2, "im", 2, 0, 0},
	{3, "in", 2, "in", 2, 0, 0},
	{3, "ir", 2, "ir", 2, 0, 0},
	{3, "il", 2, "il", 2, 0, 0},
	{3, "non", 3, "non", 3, 0, 0},
	{3, "dis", 3, "dis", 3, 0, 0},
	{3, "mis", 3, "mis", 3, 0, 0},
	{3, "re", 2, "re", 2, 0, 0},
	{3, "pre", 3, "pre", 3, 0, 0},
	{3, "any", 3, "any", 3, 0, 0},
};

static int rule_method_1(const Word_Rule_Desc *rule, const char *strIn, uint8_t strInLen, char *strOut) {

	if ((/* rule->type == 1 && */ (strInLen > 2 + rule->match_len))
	        && (strIn[strInLen - rule->match_len - 1] == strIn[strInLen - rule->match_len - 2])
	        && (memcmp(strIn + strInLen - rule->match_len, rule->match, rule->match_len) == 0)
	   ) {

		memcpy(strOut, strIn, strInLen);
		uint8_t str_len = strInLen;
		if (rule->remove && rule->remove_len > 0) {
			str_len -= rule->remove_len;
			strOut[str_len] = '\0';
		}
		if (rule->append && rule->append_len > 0) {
			memcpy(strOut + str_len, rule->append, rule->append_len);
			str_len += rule->append_len;
			strOut[str_len] = '\0';
		}
		return 0;
	}
	return -1;
}

static int rule_method_2(const Word_Rule_Desc *rule, const char *strIn, uint8_t strInLen, char *strOut) {

	if ((/* rule->type == 2 && */ (strInLen > rule->match_len))
	        && (memcmp(strIn + strInLen - rule->match_len, rule->match, rule->match_len) == 0)
	   ) {

		memcpy(strOut, strIn, strInLen);
		uint8_t str_len = strInLen;
		if (rule->remove && rule->remove_len > 0) {
			str_len -= rule->remove_len;
			strOut[str_len] = '\0';
		}
		if (rule->append && rule->append_len > 0) {
			memcpy(strOut + str_len, rule->append, rule->append_len);
			str_len += rule->append_len;
			strOut[str_len] = '\0';
		}
		return 0;
	}
	return -1;
}

static int rule_method_3(const Word_Rule_Desc *rule, const char *strIn, uint8_t strInLen, char *strOut) {
	if ((/* rule->type == 3 && */ (strInLen > rule->match_len))
	        && (memcmp(strIn, rule->match, rule->match_len) == 0)
	   ) {
		memcpy(strOut, strIn + rule->match_len, strInLen - rule->match_len + 1);
		return 0;
	}
	return -1;
}

static int rule_method(const Word_Rule_Desc *rule, const char *strIn, uint8_t strInLen, char *strOut) {
	if (rule->type == 2)
		return rule_method_2(rule, strIn, strInLen, strOut);
	if (rule->type == 3)
		return rule_method_3(rule, strIn, strInLen, strOut);
	if (rule->type == 1)
		return rule_method_1(rule, strIn, strInLen, strOut);
	return -1;
}

ExecRuleResult *execRules(const char *strIn, const Word_Rule_Desc *rules, const int ruleCnt) {
	const uint8_t strInLen = strlen(strIn);
	char wordOut[64];
	int ret = -1;
	uint8_t matchCnt = 0;
	uint8_t *itemSizeArr = (uint8_t *)RULE_MALLOC(sizeof(uint8_t) * ruleCnt);
	uint16_t data_size = 128;
	uint8_t *data = (uint8_t *)RULE_MALLOC(data_size);
	uint16_t data_copy  = 0;

	for (int i = 0; i < ruleCnt; i++) {
		ret = rule_method(rules + i, strIn, strInLen, wordOut);
		if (ret == 0) {
			uint8_t wordLen = strlen(wordOut);
			if (data_copy + wordLen > data_size) {
				data_size += (data_copy + wordLen - data_size);
				data = (uint8_t *)RULE_REALLOC(data, data_size);
			}
			itemSizeArr[matchCnt] = wordLen;
			memcpy(data + data_copy, wordOut, wordLen);
			data_copy += wordLen;
			matchCnt++;
		}
	}
	ExecRuleResult *ruleRet = NULL;

	if (matchCnt > 0) {
		char **words = (char **)RULE_MALLOC(sizeof(char *)*matchCnt);

		data_copy = 0;
		uint8_t wordLen = 0;
		for (int i = 0; i < matchCnt; i++) {
			wordLen = itemSizeArr[i];
			words[i] = (char *)RULE_MALLOC(wordLen + 1);
			memcpy(words[i], data + data_copy, wordLen);
			words[i][wordLen] = '\0';
			data_copy += wordLen;
		}

		ruleRet = (ExecRuleResult *)RULE_MALLOC(sizeof(ExecRuleResult));
		ruleRet->result = words;
		ruleRet->count = matchCnt;
	}
	RULE_FREE(data);
	RULE_FREE(itemSizeArr);
	return ruleRet;
}

/* only check the first char */
char *toLowerCase(const char *strIn) {
	const int len = strlen(strIn);
	char *word = (char *)RULE_MALLOC(len + 1);
	for (int i = 0; i < len; i++) {
		word[i] = (strIn[i] >= 'A' && strIn[i] <= 'Z') ? (strIn[i] + 32) : strIn[i];
	}
	word[len] = '\0';
	return word;
}

ExecRuleResult *execAllRules(const char *strIn) {
	const char *lowerCase = (strIn[0] >= 'A' && strIn[0] <= 'Z') ? toLowerCase(strIn) : strIn;
	ExecRuleResult *suffix_rules_result = execRules(lowerCase, Suffix_Rules, sizeof(Suffix_Rules) / sizeof(Word_Rule_Desc));
	ExecRuleResult *prefix_rules_result = execRules(lowerCase, Prefix_Rules, sizeof(Prefix_Rules) / sizeof(Word_Rule_Desc));

	int suffix_cnt = 0, prefix_cnt = 0;
	if (suffix_rules_result)
		suffix_cnt = suffix_rules_result->count;
	if (prefix_rules_result)
		prefix_cnt = prefix_rules_result->count;

	int count = 0;
	char **words = NULL;
	if (suffix_cnt > 0) {
		words = (char **)RULE_MALLOC(sizeof(char *)*suffix_cnt * sizeof(Prefix_Rules) / sizeof(Word_Rule_Desc));
		for (int i = 0; i < suffix_cnt; i++) {
			ExecRuleResult *ret = execRules(suffix_rules_result->result[i], Prefix_Rules,
			                                sizeof(Prefix_Rules) / sizeof(Word_Rule_Desc));
			if (ret) {
				if (ret->count > 0) {
					memcpy(words + count, ret->result, sizeof(char *) * (ret->count));
					count += ret->count;
					RULE_FREE(ret->result);
				}
				RULE_FREE(ret);
			}
		}
	}
	const int total_cnt = suffix_cnt + prefix_cnt + count;
	ExecRuleResult *lastRet = NULL;
	if (total_cnt > 0) {
		lastRet = (ExecRuleResult *)RULE_MALLOC(sizeof(ExecRuleResult));
		lastRet->count = total_cnt;
		lastRet->result = (char **)RULE_MALLOC(sizeof(char *)*total_cnt);
		if (suffix_cnt > 0) {
			memcpy(lastRet->result, suffix_rules_result->result, sizeof(char *)*suffix_cnt);
		}
		if (prefix_cnt > 0) {
			memcpy(lastRet->result + suffix_cnt, prefix_rules_result->result, sizeof(char *)*prefix_cnt);
		}
		if (count > 0) {
			memcpy(lastRet->result + suffix_cnt + prefix_cnt, words, sizeof(char *)*count);
		}
	}
	if (words)
		RULE_FREE(words);
	if (suffix_rules_result) {
		if (suffix_rules_result->result) {
			RULE_FREE(suffix_rules_result->result);
		}
		RULE_FREE(suffix_rules_result);
	}
	if (prefix_rules_result) {
		if (prefix_rules_result->result) {
			RULE_FREE(prefix_rules_result->result);
		}
		RULE_FREE(prefix_rules_result);
	}
	if (lowerCase != strIn) {
		RULE_FREE((void *)lowerCase);
	}
	return lastRet;
}

void freeExecRuleResult(ExecRuleResult *ptr) {
	if (ptr) {
		if (ptr->result) {
			for (int i = 0; i < ptr->count; i++) {
				if (ptr->result[i]) {
						RULE_FREE(ptr->result[i]);
				}
			}
			RULE_FREE(ptr->result);
		}
		RULE_FREE(ptr);
	}
}
