#include "client/mcp_transaction_manager.hpp"
#include "duckdb/transaction/transaction.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {

MCPTransactionManager::MCPTransactionManager(AttachedDatabase &db) : TransactionManager(db) {
}

MCPTransactionManager::~MCPTransactionManager() = default;

Transaction &MCPTransactionManager::StartTransaction(ClientContext &context) {
	lock_guard<mutex> lock(transaction_lock);

	// Create a minimal read-only transaction
	auto transaction = make_uniq<Transaction>(*this, context);
	auto &result = *transaction;
	active_transactions.push_back(std::move(transaction));

	return result;
}

ErrorData MCPTransactionManager::CommitTransaction(ClientContext &context, Transaction &transaction) {
	lock_guard<mutex> lock(transaction_lock);

	// Find and remove the transaction from active list
	for (auto it = active_transactions.begin(); it != active_transactions.end(); ++it) {
		if (it->get() == &transaction) {
			active_transactions.erase(it);
			break;
		}
	}

	// MCP transactions are read-only, so commit is always successful
	return ErrorData();
}

void MCPTransactionManager::RollbackTransaction(Transaction &transaction) {
	lock_guard<mutex> lock(transaction_lock);

	// Find and remove the transaction from active list
	for (auto it = active_transactions.begin(); it != active_transactions.end(); ++it) {
		if (it->get() == &transaction) {
			active_transactions.erase(it);
			break;
		}
	}

	// MCP transactions are read-only, so rollback is trivial
}

void MCPTransactionManager::Checkpoint(ClientContext &context, bool force) {
	// MCP databases don't have persistent state to checkpoint
	// This is a no-op for read-only MCP resources
}

} // namespace duckdb
