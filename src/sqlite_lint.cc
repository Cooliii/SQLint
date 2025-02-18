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
//  Created by liyongjie on 2017/2/23.
//  Copyright © 2017年 tencent. All rights reserved.
//

#include "sqlite_lint.h"
#include "core/lint_manager.h"
#include "core/lint_info.h"
#include <thread>

namespace sqlitelint {

    SqlExecutionDelegate kSqlExecutionDelegate;
	std::mutex log_mutex;
	std::thread::id MainThreadID;


	int SqlExecutionDelegateFunc(const char * dbPath, const char * sql, SqlExecutionCallback callback, void * para, char ** errmsg)
	{
		sqlite3* db;
		_ASSERTE(sqlite3_open(dbPath, &db) == SQLITE_OK);
		_ASSERTE(sqlite3_exec(db, sql, callback, para, errmsg) == SQLITE_OK);
		sqlite3_close(db);
		return 0;
	}

	void SetSqlExecutionDelegate(SqlExecutionDelegate func){
        kSqlExecutionDelegate = func;
    }

	void SetMainThreadID(std::thread::id MainThreadID_)
	{
		MainThreadID = MainThreadID_;
	}

	void issue_callback(const char* db_path, std::vector<sqlitelint::Issue> published_issues) {
		std::unique_lock<std::mutex> lock(log_mutex);
		std::fstream flog("../StopWatch/Log/log", std::ios::in | std::ios::out | std::ios::app);
		if (flog) {
			for (auto iter = published_issues.begin(); iter != published_issues.end(); iter++) {
				flog << "issue_id			: " << iter->id << std::endl;
				flog << "  db_path			: " << iter->db_path << std::endl;
				flog << "  table				: " << iter->table << std::endl;
				flog << "  sql statement		: " << iter->sql << std::endl;
				flog << "  issue_type		: " << iter->type << std::endl;
				flog << "  create_time		: " << iter->create_time << std::endl;
				flog << "  desc				: " << iter->desc << std::endl;
				flog << "  detail			: " << iter->detail << std::endl;
				flog << "  advice			: " << iter->advice << std::endl;
				flog << "  \n\n";
			}
			flog.close();
		}
	}

	int TraceHook(unsigned, void* dbPath, void* stmt, void*) {

		std::string* t = (std::string*)dbPath;
		char* expanded_sql = sqlite3_expanded_sql((sqlite3_stmt*)stmt);
		const char* unexpanded_sql= sqlite3_sql((sqlite3_stmt*)stmt);
		//std::vector<std::string> issues;
		//sqlitelint::NotifySqlExecutionSync(t->c_str(), sql, 100, "no info", issues);
		sqlitelint::NotifySqlExecution(t->c_str(), unexpanded_sql, 100, "no info");
		sqlite3_free(expanded_sql);
		return 0;
	}

    void InstallSQLiteLint(const char* db_path, OnPublishIssueCallback issue_callback) {
        LintManager::Get()->Install(db_path, issue_callback);
    }

    void UninstallSQLiteLint(const char* db_path) {
        std::thread uninstall_thread(&LintManager::Uninstall, LintManager::Get(), std::string(db_path));
        uninstall_thread.detach();
//        LintManager::Get()->Uninstall(db_path);
    }

	void NotifySqlExecution(const char* db_path, const char* sql, long time_cost, const char* ext_info) {
        return LintManager::Get()->NotifySqlExecution(db_path, sql, time_cost, ext_info);
    }

	void NotifySqlExecutionSync(const char* db_path, const char* sql, long time_cost, const char* ext_info, std::vector<std::string>& issues_id) {
		return LintManager::Get()->NotifySqlExecutionSync(db_path, sql, time_cost, ext_info, issues_id);
	}

    void SetWhiteList(const char* db_path, const std::map<std::string, std::set<std::string>>& white_list) {
        LintManager::Get()->SetWhiteList(db_path, white_list);
    }

    void EnableChecker(const char* db_path, const std::string& checker_name) {
        LintManager::Get()->EnableChecker(db_path, checker_name);
    }

	void InitCheck() {
		LintManager::Get()->InitCheck();
	}

	void InitCheck(const char * db_path) {
		LintManager::Get()->InitCheck(db_path);
	}
}
