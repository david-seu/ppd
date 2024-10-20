# Synchronization Rules and Mutex Usage

## Mutexes Used and Their Purpose

### Account Mutex (`Account::mtx`)

- **Description**: Each `Account` object has its own mutex, `mtx`.
- **Purpose**: Protects access to the account's `balance` and `log`.
- **Invariants Protected**:
  - **Balance Consistency**: The account's `balance` accurately reflects all transactions that have been applied.
  - **Log Integrity**: The `log` contains a complete and accurate record of all transactions involving the account.

## Synchronization Rules

### Transfer Operation (`transfer` function)

- **Mutexes Involved**: `from_account.mtx` and `to_account.mtx`.
- **Locking Order**:
  - To prevent deadlocks, mutexes are always locked in a consistent order based on account IDs.
    - If `from_id < to_id`, lock `from_account.mtx` first, then `to_account.mtx`.
    - Else, lock `to_account.mtx` first, then `from_account.mtx`.
- **Operations Protected**:
  - Updating `from_account.balance` and `to_account.balance`.
  - Appending the transaction record to both `from_account.log` and `to_account.log`.
- **Invariants Maintained**:
  - **Atomicity of Transfers**: Ensures that both the balance updates and log entries are applied together, or not at all.
  - **Consistency Between Accounts**: The transaction is recorded identically in both accounts involved.

### Consistency Check (`perform_consistency_check` function)

- **Mutexes Involved**: All `Account::mtx` mutexes.
- **Locking Strategy**:
  - All account mutexes are locked before performing the consistency check.
- **Operations Protected**:
  - Calculating the expected balance for each account based on the initial balance and transaction logs.
  - Verifying that every transaction in an account's log is also present in the log of the other account involved.
- **Invariants Maintained**:
  - **Balance Accuracy**: The account's balance matches the sum of the initial balance and net transactions.
  - **Log Consistency**: All transaction records are mutual between accounts.

## Deadlock Prevention

- **Consistent Lock Ordering**:
  - Mutexes are always locked in the same order based on account IDs.
  - This strategy prevents circular wait conditions, avoiding deadlocks.

## Invariants Protected by Mutexes

1. **Balance Invariant**:
   - The `balance` of an account reflects all transactions applied to it.
   - Protected by `Account::mtx` during balance updates and reads.

2. **Log Invariant**:
   - The `log` of an account contains all transactions involving the account.
   - Protected by `Account::mtx` during log updates and reads.

3. **Transaction Atomicity Invariant**:
   - Transactions are applied atomically; both balances and logs are updated together.
   - Ensured by locking both accounts' mutexes during a transfer operation.

4. **Mutual Transaction Recording**:
   - For each transaction, both accounts involved have the transaction recorded in their logs.
   - Ensured within the locked region in the `transfer` function.

