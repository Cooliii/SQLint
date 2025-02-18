/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created by wuzhiwen on 2017/3/8.
//

#include "select_tree_helper.h"
#include "comm/log/logger.h"

namespace sqlitelint {

	SelectTreeHelper::SelectTreeHelper(Select *select) : has_using_or_on(false)
            ,has_bit_operation(false),has_fuzzy_matching(false),has_and(false),has_or(false),has_in(false){
		select_ = select;
		columns_to_check_index.clear();
	}

	void SelectTreeHelper::Process() {
		ProcessSelect(select_);
		sVerbose("SelectTreeHelper::init done");
	}

    Select* SelectTreeHelper::GetSelect(const std::string& table) {
		std::vector<Select*> selects = select_tree_map_[table];
		if (selects.size() > 0) {
			sVerbose("getSelect: table=%s", table.c_str());
			Select* select = selects[0];
			selects.erase(selects.begin());
			return select;
		}

		sWarn("SelectTreeHelper: not find select tree, table=%s", table.c_str());
		return nullptr;
    }

	void SelectTreeHelper::ProcessSelect(Select *p) {
		if (!p)return;
		if (p->pPrior) {
			ProcessSelect(p->pPrior);
		}
		ProcessExprList(p->pEList);
		if (p->pSrc&&p->pSrc->nSrc) {
			ProcessSrcList(p->pSrc, p);
		}
		if (p->pWhere) {
			ProcessExpr(p->pWhere);
		}
		if (p->pGroupBy) {
			ProcessExprList(p->pGroupBy);
		}
		if (p->pOrderBy) {
			ProcessExprList(p->pOrderBy);
		}
		if (p->pHaving) {
			ProcessExpr(p->pHaving);
		}
		if (p->pLimit) {
			ProcessExpr(p->pLimit);
		}
		if (p->pOffset) {
			ProcessExpr(p->pOffset);
		}
	}

	void SelectTreeHelper::ProcessExprList(const ExprList *p) {
		if (!p)return;
		for (int i = 0; i < p->nExpr; i++) {
			ProcessExpr(p->a[i].pExpr);
		}
	}

	void SelectTreeHelper::ProcessExpr(const Expr *p) {
		if (!p)return;

		ProcessExpr(p->pLeft);
		ProcessToken(p, p->op);
		ProcessExpr(p->pRight);

		if (p->pSelect) {
			ProcessSelect(p->pSelect);
		}
		ProcessExprList(p->pList);
	}

	//from
	void SelectTreeHelper::ProcessSrcList(const SrcList *p, Select *s) {
		if (!p)return;
		for (int i = 0; i < p->nSrc; i++) {
			AddSelectTree(p->a[i].zName, s);

			ProcessSelect(p->a[i].pSelect);

            if (p->a[i].pUsing) {
                has_using_or_on = true;
            }

			if (p->a[i].pOn) {
                has_using_or_on = true;
				ProcessExpr(p->a[i].pOn);
			}
		}
	}

    void SelectTreeHelper::ProcessToken(const Expr *p, int op) {

        switch (op) {
            case TK_BITAND:
            case TK_BITOR:
            case TK_BITNOT:
            case TK_LSHIFT:
            case TK_RSHIFT:
                has_bit_operation = true;
                break;
            case TK_LIKE_KW:
                has_fuzzy_matching = true;
                break;
			case TK_AND:
				if (!p->pParent || (p->pParent && p->pParent->op != TK_AND)) {
					std::vector<std::string> parallelColomuns;
					parallelColomuns.clear();
					GetColumnsConnectedByAnd(p, parallelColomuns);
					if (parallelColomuns.size() > 1) {
						columns_to_check_index.push_back(parallelColomuns);
						if (!has_and)
							has_and = true;
					}
				}
				break;
            case TK_OR:
                has_or = true;
                break;
            case TK_IN:
                has_in = true;
                break;
        }
    }

	inline bool SelectTreeHelper::checkOP(int op)
	{
		if (op == TK_IS || op == TK_IN || op == TK_ISNULL || op == TK_EQ || op == TK_GT || op == TK_LE || op == TK_LT || op == TK_GE)
			return true;
		else
			return false;
	}

	inline std::string SelectTreeHelper::getIDfromExpr(const Expr* p)
	{
		if (p->op == TK_ID) {
			const Token& token = p->token;
			char* ID = (char*)malloc(sizeof(char)*(token.n) + 1);
			strncpy(ID, (const char*)token.z, token.n);
			ID[token.n] = 0;
			return (std::string) ID;
		}
		else if (p->op == TK_DOT)
			return getIDfromExpr(p->pLeft) + "." + getIDfromExpr(p->pRight);
		else
			return "";
	}

	void SelectTreeHelper::GetColumnsConnectedByAnd(const Expr * p, std::vector<std::string>& parallelColomuns)
	{
		if (!p) return;
		if (p->op == TK_AND) {
			GetColumnsConnectedByAnd(p->pLeft, parallelColomuns);
			GetColumnsConnectedByAnd(p->pRight, parallelColomuns);
		}
		else if (checkOP(p->op) && p->pLeft) {
			parallelColomuns.push_back(getIDfromExpr(p->pLeft));
		}
	}

	void SelectTreeHelper::AddSelectTree(const char *table, Select *p) {
        if(!table){
            return;
        }
		if (select_tree_map_.find(table) == select_tree_map_.end()) {
			std::vector<Select*> selects;
			select_tree_map_[table] =  selects;
		}
		select_tree_map_[table].push_back(p);
	}

	SelectTreeHelper::~SelectTreeHelper() {
        for(auto it = select_tree_map_.begin();it != select_tree_map_.end();it++) {
            it->second.clear();
        }
        select_tree_map_.clear();
	}

    bool SelectTreeHelper::HasUsingOrOn() {
        return has_using_or_on;
    }

    bool SelectTreeHelper::HasFuzzyMatching() {
        return has_fuzzy_matching;
    }


    bool SelectTreeHelper::HasBitOperation() {
        return has_bit_operation;
    }

	bool SelectTreeHelper::HasAnd() {
		return has_and;
	}

    bool SelectTreeHelper::HasOr() {
        return has_or;
    }

    bool SelectTreeHelper::HasIn() {
        return has_in;
    }
	const std::vector<std::vector<std::string>>& SelectTreeHelper::getColumns()
	{
		return columns_to_check_index;
	}
}
