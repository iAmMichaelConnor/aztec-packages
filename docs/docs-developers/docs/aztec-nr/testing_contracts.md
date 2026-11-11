---
title: Testing Contracts
tags: [contracts, tests, testing, noir]
keywords: [tests, testing, noir]
sidebar_position: 6
description: Write and run tests for your Aztec smart contracts using Noir's TestEnvironment.
references: ["labs/noir-projects/aztec-nr/aztec/src/test/*", "labs/yarn-project/aztec/src/cli/cmds/start_txe.ts", "labs/yarn-project/aztec/src/cli/cmds/compile.ts", "labs/yarn-project/aztec/scripts/aztec.sh"]
---

This guide shows you how to test your Aztec smart contracts using Noir's `TestEnvironment` for fast, lightweight testing.

## Prerequisites

- An Aztec contract project with functions to test
- Basic understanding of Noir syntax

:::tip
For complex cross-chain or integration testing, see the [TypeScript testing guide](../aztec-js/how_to_test.md). For a worked example, [Testing the Pod Racing contract](../tutorials/js_tutorials/webapp/07-testing-the-contract.md) tests a two-player game: setup helpers, failure cases and one-time guards.
:::

## Write Aztec contract tests

Use `TestEnvironment` from `aztec-nr` for contract unit testing:

- **Fast**: Lightweight environment with mocked components
- **Convenient**: Similar to Foundry for simple contract tests
- **Limited**: No rollup circuits or cross-chain messaging

For complex end-to-end tests, use [TypeScript testing](../aztec-js/how_to_test.md) with `aztec.js`.

## Run your tests

Execute Aztec Noir tests using:

```bash
aztec test
```

### Test execution process

`aztec test` does three things:

1. Compiles the contracts, unless their artifacts are up to date
2. Starts the test environment
3. Runs `nargo test` against it, with the arguments you passed

:::warning
Always use `aztec test` instead of `nargo test`. The `TestEnvironment` requires the test environment oracle resolver provided by the `aztec` CLI.
:::

### Test a single package

```bash
aztec test --package my_project_test
```

With `--package`, only the contracts that package depends on are compiled, each only if its sources changed. A test crate that deploys no contract compiles none. Without it, every contract in the workspace is compiled first.

A test crate must therefore list the contracts it deploys in its `Nargo.toml`, as the crate scaffolded by `aztec new` does:

```toml
[dependencies]
aztec = { git="https://github.com/aztec-labs-eng/aztec-nr", tag="#include_aztec_version", directory="aztec" }
my_project_contract = { path = "../my_project_contract" }
```

If a test deploys a contract of the workspace that its crate does not depend on, the deployment fails with an error that names the dependency to add.

### Skip the circuits with `--unconstrained`

```bash
aztec test --unconstrained
```

Compiling a private function into a circuit takes time in proportion to the size of the circuit: minutes for a function that verifies a signature or a proof. Tests only execute the function; nothing is proven. With `--unconstrained`, contracts are compiled with `aztec compile --unconstrained`, which compiles private functions as unconstrained code in seconds, and the tests deploy those builds.

An unconstrained build is for tests only:

- It is written to `target/unconstrained`, next to the regular build in `target`, and has no verification keys. It cannot be registered or deployed, and tooling other than the test environment refuses to load it.
- A contract's class id and address differ from those of the regular build, so tests must not rely on a fixed address.
- Code that branches on `is_unconstrained()` takes its unconstrained path.
- Errors that only a circuit reports, such as a loop bound that is not a constant, are not caught.

Use it while developing, and keep a regular `aztec compile` or `aztec test` in CI.

## Keep tests in the test crate

`aztec new` and `aztec init` scaffold a workspace with two crates: a contract crate and a separate test crate. For `aztec new my_project`, these are `my_project_contract` and `my_project_test`. Keep all `#[test]` functions in the test crate, not in the contract crate.

If tests end up inside a contract crate, `aztec compile` emits a warning:

```text
WARNING: Found tests in contract crate(s):
  my_project_contract::test_something
Tests should be in a dedicated test crate, not in the contract crate.
```

The reason is **unnecessary recompilation**: a contract's compiled artifact depends on everything in its crate, so a test-only edit forces the contract to recompile even though its logic has not changed. Keeping tests in the separate test crate lets `aztec test` skip contract recompilation when only test code changed.

## Basic test structure

`aztec new my_project` scaffolds a workspace with two crates: a `contract` crate that holds the contract code, and a separate `test` crate that holds your `#[test]` functions:

```text
my_project/
├── Nargo.toml                 # [workspace] members = ["my_project_contract", "my_project_test"]
├── my_project_contract/
│   ├── Nargo.toml             # type = "contract"
│   └── src/main.nr
└── my_project_test/
    ├── Nargo.toml             # type = "lib", depends on my_project_contract
    └── src/lib.nr             # #[test] functions go here
```

The motivation for the split of contract and tests into its own crates is **faster iteration**: editing a test does not invalidate the contract's compiled artifact, so `aztec test` skips contract recompilation when only test code changed.

`aztec compile` warns if it finds `#[test]` functions inside a contract crate.

The generated test crate template imports the contract by package name and then initializes it:

```rust
// my_project_test/src/lib.nr
use aztec::test::helpers::test_environment::TestEnvironment;
use my_project_contract::Main;

#[test]
unconstrained fn test_constructor() {
    let mut env = TestEnvironment::new();
    let deployer = env.create_light_account();

    let _contract_address = env.deploy("@my_project_contract/Main")
        .with_private_initializer(deployer, Main::interface().constructor());
}
```

Because tests live in their own crate, we refer to the contract via its crate name using the `@crate_name/ContractName` syntax.

:::info Test execution notes

- Tests run in parallel by default
- Use `unconstrained` functions for faster execution
- See all `TestEnvironment` methods [here](pathname:///aztec-nr-api/#api_ref_version/noir_aztec/test/helpers/test_environment/struct.TestEnvironment)
- It is always necessary to deploy a contract in order to test it
:::

If you'll add arguments to your contract's constructor you pass them directly to the constructor function in the test:

```rust
let initializer = MyContract::interface().constructor(param1, param2);
```

Since Aztec contracts can be initialized both in private and public or they can be interacted with without any kind of initialization (see [Contract creation](../foundational-topics/contract_creation.md) for how Aztec's deployment model differs from Ethereum's) there are 3 options on the deployer:

```rust
let contract_address = deployer.with_private_initializer(owner, initializer);
let contract_address = deployer.with_public_initializer(owner, initializer);
let contract_address = deployer.without_initializer();
```

:::tip Reusable setup functions
Create a setup function to avoid repeating initialization code:

```rust
pub unconstrained fn setup(initial_value: Field) -> (TestEnvironment, AztecAddress, AztecAddress) {
    let mut env = TestEnvironment::new();
    let owner = env.create_light_account();
    let initializer = MyContract::interface().constructor(initial_value, owner);
    let contract_address = env.deploy("@my_project_contract/MyContract").with_private_initializer(owner, initializer);
    (env, contract_address, owner)
}

#[test]
unconstrained fn test_something() {
    let (env, contract_address, owner) = setup(42);
    // Your test logic here
}
```

:::

## Calling contract functions

TestEnvironment provides methods for different function types:

### Private functions

```rust
// Call private function
env.call_private(caller, Token::at(token_address).transfer(recipient, 100));

// Returns the result
let result = env.call_private(owner, Contract::at(address).get_private_data());
```

### Public functions

```rust
// Call public function
env.call_public(caller, Token::at(token_address).mint_to_public(recipient, 100));

// View public state (read-only)
let balance = env.view_public(Token::at(token_address).balance_of_public(owner));
```

### Utility/Unconstrained functions

```rust
// Simulate utility/view functions (unconstrained)
let total = env.execute_utility(Token::at(token_address).balance_of_private(owner));

// To set the `msg_sender` the utility function observes, use the `_opts` variant
let secret = env.execute_utility_opts(
    ExecuteUtilityOptions::new().with_from(caller),
    Registry::at(registry_address).get_app_siloed_secret(sender, recipient),
).map(|secrets| secrets.shared);
```

:::tip Helper function pattern
Create helper functions for common assertions:

```rust
pub unconstrained fn check_balance(
    env: TestEnvironment,
    token_address: AztecAddress,
    owner: AztecAddress,
    expected: u128,
) {
    assert_eq(
        env.execute_utility(Token::at(token_address).balance_of_private(owner)),
        expected
    );
}
```

:::

## Testing logic without a contract

`call_private` and `call_public` run a deployed contract, which has to be compiled first. Most logic can be tested without one: `TestEnvironment` can run code in a private, public or utility context, as if it were inside a contract function. If your contract functions are thin wrappers around library functions or state variables, test those directly.

```rust
#[test]
unconstrained fn only_the_admin_can_mint() {
    let mut env = TestEnvironment::new();
    let admin = env.create_light_account();

    env.private_context_opts(PrivateContextOptions::new().with_from(admin), |context| {
        // The same function the contract's `mint` calls
        mint(context, STORAGE_SLOT, admin, 100);
    });
}
```

`private_context`, `public_context` and `utility_context` take a closure that receives the context. The `_at` variants set the contract address the context runs in, and the `_opts` variants take further options:

| Option | Effect |
| --- | --- |
| `PrivateContextOptions::at_contract_address(address)` | The address notes, nullifiers and storage are siloed with |
| `PrivateContextOptions::with_from(address)` | The address the context runs on behalf of: the `msg_sender`, the default sender for tags, and the only address whose notes and keys are accessible, as with the `from` of `call_private` |
| `PrivateContextOptions::with_additional_scopes([addresses])` | Further addresses whose notes and keys are accessible, including addresses that are not accounts |
| `PrivateContextOptions::at_anchor_block_number(number)` | A past block to anchor the context to |
| `PublicContextOptions::at_contract_address(address)`, `with_from(address)` | The same for `public_context_opts` |

Without `with_from`, the `msg_sender` is the zero address and the notes of all test accounts are accessible.

```rust
#[test(should_fail_with = "caller is not the admin")]
unconstrained fn others_cannot_set_the_fee() {
    let mut env = TestEnvironment::new();
    let admin = env.create_light_account();
    let other = env.create_light_account();

    env.public_context_opts(PublicContextOptions::new().with_from(other), |context| {
        set_fee(context, STORAGE_SLOT, admin, 5);
    });
}
```

A note created in a private context is not known to any account until its message is processed. Pass the message the code returns to `discover_note`:

```rust
let note_message = env.private_context(|context| create_note(context, owner, STORAGE_SLOT, note));
env.discover_note(note_message);
```

:::info What still needs a deployed contract

- Calls between contracts, and a private function that enqueues a public one: a context cannot make contract calls.
- Messages delivered with constrained onchain delivery, or to a recipient whose keys are not in scope: both make a contract call.
- Finding a note through the contract's own message processing: messages delivered onchain from a context are not included in the block it mines.
- Anything about the contract's interface: argument order, initializers, `#[only_self]`, and checks written in the contract function itself.

For these, deploy the contract, and use `aztec test --unconstrained` to keep the compile fast.

:::

## Creating accounts

Two types of accounts are available:

```rust
// Light account - fast, limited features
let owner = env.create_light_account();

// Contract account - full features, slower
let owner = env.create_contract_account();
```

:::info Account type comparison
**Light accounts:**

- Fast to create
- Work for simple transfers and tests
- Cannot process authwits
- No account contract deployed

**Contract accounts:**

- Required for authwit testing
- Support account abstraction features
- Slower to create (deploys account contract)
- Needed for cross-contract authorization
  :::

:::tip Choosing account types

```rust
pub unconstrained fn setup(with_authwits: bool) -> (TestEnvironment, AztecAddress, AztecAddress) {
    let mut env = TestEnvironment::new();
    let (owner, recipient) = if with_authwits {
        (env.create_contract_account(), env.create_contract_account())
    } else {
        (env.create_light_account(), env.create_light_account())
    };
    // ... deploy contracts ...
    (env, owner, recipient)
}
```

:::

## Testing with authwits

[Authwits](./framework-description/authentication_witnesses.md) allow one account to authorize another to act on its behalf.

:::warning
Authwits require **contract accounts**, not light accounts.
:::

### Import authwit helpers

```rust
use aztec::test::helpers::authwit::{
    add_private_authwit_from_call,
    add_public_authwit_from_call,
};
```

### Private authwits

```rust
#[test]
unconstrained fn test_private_authwit() {
    // Setup with contract accounts (required for authwits)
    let (env, token_address, owner, spender) = setup(true);

    // Create the call that needs authorization
    let amount = 100;
    let nonce = 7; // Non-zero nonce for authwit
    let burn_call = Token::at(token_address).burn_private(owner, amount, nonce);

    // Grant authorization from owner to spender
    add_private_authwit_from_call(env, owner, spender, burn_call);

    // Spender can now execute the authorized action
    env.call_private(spender, burn_call);
}
```

### Public authwits

```rust
#[test]
unconstrained fn test_public_authwit() {
    let (env, token_address, owner, spender) = setup(true);

    // Create public action that needs authorization
    let transfer_call = Token::at(token_address).transfer_in_public(owner, recipient, 100, nonce);

    // Grant public authorization
    add_public_authwit_from_call(env, owner, spender, transfer_call);

    // Execute with authorization
    env.call_public(spender, transfer_call);
}
```

## Time traveling

Contract calls do not advance the timestamp by default, despite each of them resulting in a block with a single transaction. Block timestamp can instead be manually manipulated by any of the following methods:

```rust
// Sets the timestamp of the next block to be mined, i.e. of the next public execution. Does not affect private execution.
env.set_next_block_timestamp(block_timestamp);

// Same as `set_next_block_timestamp`, but moving time forward by `duration` instead of advancing to a target timestamp.
env.advance_next_block_timestamp_by(duration);

// Mines an empty block at a given timestamp, causing the next public execution to occur at this time (like `set_next_block_timestamp`), but also allowing for private execution to happen using this empty block as the anchor block.
env.mine_block_at(block_timestamp);
```

## Testing failure cases

Test functions that should fail using annotations:

### Generic failure

```rust
#[test(should_fail)]
unconstrained fn test_unauthorized_access() {
    let (env, contract, owner) = setup(false);
    let attacker = env.create_light_account();

    // This should fail because attacker is not authorized
    env.call_private(attacker, Contract::at(contract).owner_only_function());
}
```

### Specific error message

```rust
#[test(should_fail_with = "Balance too low")]
unconstrained fn test_insufficient_balance() {
    let (env, token, owner, recipient) = setup(false);

    // Try to transfer more than available
    let balance = 100;
    let transfer_amount = 101;

    env.call_private(owner, Token::at(token).transfer(recipient, transfer_amount));
}
```

### Testing authwit failures

```rust
#[test(should_fail_with = "Unknown auth witness for message hash")]
unconstrained fn test_missing_authwit() {
    let (env, token, owner, spender) = setup(true);

    // Try to burn without authorization
    let burn_call = Token::at(token).burn_private(owner, 100, 1);

    // No authwit granted - this should fail
    env.call_private(spender, burn_call);
}

```

## Test environment oracle versioning

The test environment uses an oracle interface to communicate between your Noir test code and the `aztec test` CLI. This interface is versioned so that mismatches between the Aztec.nr dependency used to compile the test and the CLI version are detected automatically.

The version uses two components, `major.minor`, with the same compatibility rules as [PXE oracle versioning](../foundational-topics/pxe/index.md#oracle-versioning):

- **`major`** must match exactly. A major bump means oracles were removed or had their signatures changed, and a test environment on a different major cannot safely run the test.
- **`minor`** indicates additive changes (new oracles). The test environment uses a best-effort approach: a test compiled against a higher `minor` is still allowed to run, and an error is only thrown if the test actually invokes an oracle the test environment does not know about.

### Resolving a version mismatch

If you see an error like _"Incompatible test environment version: The test was compiled with a newer version of Aztec.nr than your test environment supports"_, the test uses oracles from a newer Aztec.nr than your `aztec test` CLI supports.

To fix it, make sure your `aztec` CLI version and the `aztec` dependency in the test crate's `Nargo.toml` are on the same release. Note that the test crate's Aztec.nr version can differ from the contract crate's version, depending on your project configuration. For example, if your CLI is on `#include_aztec_version`, the test crate's `Nargo.toml` should reference the matching tag:

```toml
[dependencies]
aztec = { git="https://github.com/aztec-labs-eng/aztec-nr", tag="#include_aztec_version", directory="aztec" }
```

If the test environment reports a version that _should_ include every oracle the test needs but an oracle is still missing, this is likely a bug rather than a version problem.
