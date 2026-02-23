#pragma once

#include "duckdb.hpp"
#include "duckdb/transaction/transaction_manager.hpp"

namespace duckdb {

#ifndef __EMSCRIPTEN__

//! Minimal MCP transaction manager for read-only operations
//! MCP resources are read-only, so this provides minimal transaction support
class MCPTransactionManager : public TransactionManager {
public:
	explicit MCPTransactionManager(AttachedDatabase &db);
	~MCPTransactionManager() override;

	// Core transaction operations
	Transaction &StartTransaction(ClientContext &context) override;
	ErrorData CommitTransaction(ClientContext &context, Transaction &transaction) override;
	void RollbackTransaction(Transaction &transaction) override;
	void Checkpoint(ClientContext &context, bool force = false) override;

	bool IsDuckTransactionManager() override {
		return false;
	}

private:
	mutex transaction_lock;
	vector<unique_ptr<Transaction>> active_transactions;
	transaction_t next_transaction_id = 1;
};

#endif // !__EMSCRIPTEN__

} // namespace duckdb
