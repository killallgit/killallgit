use anyhow::{anyhow, Context, Result};
use clap::{Parser, Subcommand};
use dialoguer::{console::style, MultiSelect, Select};
use regex::Regex;
use std::fs;
use std::io::IsTerminal;
use std::path::Path;
use std::process::Command;

#[derive(Parser)]
#[command(
    name = "killallgit",
    about = "A CLI tool for managing git worktrees and branches",
    version,
    propagate_version = true
)]
struct Cli {
    #[command(subcommand)]
    command: Option<Commands>,
}

#[derive(Subcommand)]
enum Commands {
    /// Add a resource
    Add {
        #[command(subcommand)]
        resource: AddResource,
    },
    /// Clean up resources
    Clean {
        #[command(subcommand)]
        resource: CleanResource,
    },
}

#[derive(Subcommand)]
enum AddResource {
    /// Add a new worktree
    Worktree {
        /// Name of the worktree
        name: String,
    },
}

#[derive(Subcommand)]
enum CleanResource {
    /// Clean up worktrees
    Worktrees {
        /// Regex pattern to match worktree names (optional, interactive if not provided)
        #[arg(conflicts_with = "all")]
        pattern: Option<String>,
        /// Force removal even if there are uncommitted changes
        #[arg(long)]
        force: bool,
        /// Remove all worktrees
        #[arg(long)]
        all: bool,
        /// List matching worktrees without deleting
        #[arg(long)]
        dry_run: bool,
        /// Output as JSON array (requires --dry-run)
        #[arg(long, requires = "dry_run")]
        json: bool,
    },
    /// Clean up branches (local or remote)
    Branches {
        /// Pattern to match branches (e.g., "feat/.*" for local, "origin/feat/.*" for remote)
        pattern: Option<String>,
        /// Skip confirmation prompts
        #[arg(long)]
        force: bool,
        /// List matching branches without deleting
        #[arg(long)]
        dry_run: bool,
        /// Output as JSON array (requires --dry-run)
        #[arg(long, requires = "dry_run")]
        json: bool,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    match cli.command {
        Some(Commands::Add { resource }) => match resource {
            AddResource::Worktree { name } => add_worktree(name),
        },
        Some(Commands::Clean { resource }) => match resource {
            CleanResource::Worktrees {
                pattern,
                force,
                all,
                dry_run,
                json,
            } => {
                if all {
                    clean_all_worktrees(force, dry_run, json)
                } else {
                    clean_worktrees(pattern, force, dry_run, json)
                }
            }
            CleanResource::Branches {
                pattern,
                force,
                dry_run,
                json,
            } => clean_branches(pattern, force, dry_run, json),
        },
        None => interactive_menu(),
    }
}

fn interactive_menu() -> Result<()> {
    use dialoguer::Input;

    if !std::io::stdin().is_terminal() {
        return Err(anyhow!(
            "Interactive mode requires a terminal. Use command-line arguments instead.\n\
             Run 'killallgit --help' for available commands."
        ));
    }

    loop {
        let main_options = &[
            style("Add").cyan().to_string(),
            style("Clean").cyan().to_string(),
            style("Exit").dim().to_string(),
        ];

        let selection = Select::new()
            .with_prompt(style("What would you like to do?").bold().to_string())
            .items(main_options)
            .interact_opt()?;

        match selection {
            Some(0) => {
                // Add submenu
                let add_options = &[
                    style("Worktree").yellow().to_string(),
                    style("← Back").dim().to_string(),
                ];

                let add_sel = Select::new()
                    .with_prompt(style("Add what?").bold().to_string())
                    .items(add_options)
                    .interact_opt()?;

                match add_sel {
                    Some(0) => {
                        let name: String = Input::new()
                            .with_prompt(style("Worktree name").cyan().to_string())
                            .interact_text()?;
                        add_worktree(name)?;
                        break;
                    }
                    _ => continue,
                }
            }
            Some(1) => {
                // Clean submenu
                let clean_options = &[
                    style("Worktrees").yellow().to_string(),
                    style("Branches").yellow().to_string(),
                    style("← Back").dim().to_string(),
                ];

                let clean_sel = Select::new()
                    .with_prompt(style("Clean what?").bold().to_string())
                    .items(clean_options)
                    .interact_opt()?;

                match clean_sel {
                    Some(0) => {
                        clean_worktrees(None, false, false, false)?;
                        break;
                    }
                    Some(1) => {
                        clean_branches(None, false, false, false)?;
                        break;
                    }
                    _ => continue,
                }
            }
            Some(2) | None => break,
            _ => break,
        }
    }

    Ok(())
}

// ============================================================================
// Shared Utilities
// ============================================================================

fn get_git_root() -> Result<std::path::PathBuf> {
    let output = Command::new("git")
        .args(["rev-parse", "--show-toplevel"])
        .output()
        .context("Failed to execute git rev-parse --show-toplevel")?;

    if !output.status.success() {
        return Err(anyhow!("Not in a git repository"));
    }

    let path = String::from_utf8_lossy(&output.stdout).trim().to_string();
    Ok(std::path::PathBuf::from(path))
}

fn get_repo_name() -> Result<String> {
    let git_root = get_git_root()?;
    let repo_name = git_root
        .file_name()
        .and_then(|name| name.to_str())
        .ok_or_else(|| anyhow!("Could not determine repository name"))?;

    Ok(repo_name.to_string())
}

fn get_worktree_base_path() -> Result<std::path::PathBuf> {
    let git_root = get_git_root()?;
    let repo_name = get_repo_name()?;
    let parent = git_root
        .parent()
        .ok_or_else(|| anyhow!("Could not determine parent directory"))?;
    Ok(parent.join(".worktrees").join(repo_name))
}

fn get_worktree_path(worktree_name: &str) -> Result<std::path::PathBuf> {
    Ok(get_worktree_base_path()?.join(worktree_name))
}

fn print_json_array(items: &[&str]) {
    println!(
        "{}",
        serde_json::to_string(&items).expect("serialization of string slice should never fail")
    );
}

fn confirm_deletion(item_type: &str, count: usize, target: &str) -> Result<bool> {
    let confirmation = Select::new()
        .with_prompt(
            style(format!(
                "⚠️  Delete these {} {} from {}?",
                count, item_type, target
            ))
            .red()
            .bold()
            .to_string(),
        )
        .items(&[
            style("❌ No, cancel").red().to_string(),
            style("💥 Yes, delete them!").red().bold().to_string(),
        ])
        .interact()?;

    Ok(confirmation == 1)
}

/// Handles empty result output for dry-run and json modes
fn handle_empty_result(json: bool, dry_run: bool, message: &str) {
    if json {
        print_json_array(&[]);
    } else if !dry_run {
        println!("{}", style(message).yellow());
    }
}

/// Outputs items for dry-run mode (json or plain text)
fn output_dry_run(items: &[&str], json: bool) {
    if json {
        print_json_array(items);
    } else {
        for item in items {
            println!("{}", item);
        }
    }
}

/// Displays protected items that will be skipped
fn display_protected_items(items: &[&str], label: &str) {
    if !items.is_empty() {
        println!("{}", style(format!("⚠️  Protected {} (skipped):", label)).yellow());
        for item in items {
            println!("  {} {}", style("•").dim(), style(*item).dim());
        }
        println!();
    }
}

/// Displays items that will be deleted
fn display_items_for_deletion(items: &[&str], item_type: &str, context: &str) {
    println!(
        "{}",
        style(format!("Found {} {} {}:", items.len(), item_type, context)).cyan()
    );
    for item in items {
        println!("  • {}", style(*item).yellow().bold());
    }
}

/// Interactive multi-select with styled items. Returns None if cancelled or empty.
fn interactive_multi_select<'a>(
    items: &'a [&str],
    prompt: &str,
    item_type: &str,
) -> Result<Option<Vec<&'a str>>> {
    let styled_items: Vec<String> = items
        .iter()
        .map(|name| style(*name).yellow().bold().to_string())
        .collect();

    let selections = MultiSelect::new()
        .with_prompt(style(prompt).cyan().to_string())
        .items(&styled_items)
        .interact_opt()?;

    match selections {
        Some(s) if !s.is_empty() => Ok(Some(s.iter().map(|&i| items[i]).collect())),
        Some(_) => {
            println!("{}", style(format!("No {} selected.", item_type)).yellow());
            Ok(None)
        }
        None => {
            println!("{}", style("Selection cancelled.").yellow());
            Ok(None)
        }
    }
}

/// Prints deletion summary after batch operations
fn print_deletion_summary(deleted: usize, failed: usize, item_type: &str, target: &str) {
    println!();
    println!(
        "{}",
        style(format!("✓ Deleted {} {} from {}", deleted, item_type, target))
            .green()
            .bold()
    );
    if failed > 0 {
        println!(
            "{}",
            style(format!("✗ Failed to delete {} {}", failed, item_type)).red()
        );
    }
}

/// Result of a git command execution
enum GitCommandResult {
    Success,
    Failure(String),
}

/// Execute a git command and return success/failure with error message
fn execute_git_command(args: &[&str]) -> GitCommandResult {
    match Command::new("git").args(args).output() {
        Ok(out) if out.status.success() => GitCommandResult::Success,
        Ok(out) => {
            let err = String::from_utf8_lossy(&out.stderr).trim().to_string();
            GitCommandResult::Failure(err)
        }
        Err(e) => GitCommandResult::Failure(e.to_string()),
    }
}

/// Prints the result of a deletion attempt (checkmark or X with error)
fn print_deletion_result(result: &GitCommandResult) {
    match result {
        GitCommandResult::Success => println!("{}", style("✓").green()),
        GitCommandResult::Failure(err) => {
            println!("{} {}", style("✗").red(), style(err).dim())
        }
    }
}

// ============================================================================
// Branch Scope and Pattern Parsing
// ============================================================================

enum BranchScope {
    Local,
    Remote { remote: String },
}

fn get_configured_remotes() -> Result<Vec<String>> {
    let output = Command::new("git")
        .args(["remote"])
        .output()
        .context("Failed to execute git remote")?;

    if !output.status.success() {
        return Ok(vec![]);
    }

    let remotes: Vec<String> = String::from_utf8_lossy(&output.stdout)
        .lines()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .collect();

    Ok(remotes)
}

fn parse_branch_pattern(pattern: &str) -> Result<(BranchScope, String)> {
    let remotes = get_configured_remotes()?;

    for remote in remotes {
        let prefix = format!("{}/", remote);
        if pattern.starts_with(&prefix) {
            let branch_pattern = pattern.strip_prefix(&prefix).unwrap().to_string();
            return Ok((BranchScope::Remote { remote }, branch_pattern));
        }
    }

    Ok((BranchScope::Local, pattern.to_string()))
}

// ============================================================================
// Worktree Functions
// ============================================================================

fn add_worktree(name: String) -> Result<()> {
    let worktree_path = get_worktree_path(&name)?;
    let base_path = get_worktree_base_path()?;

    fs::create_dir_all(&base_path)
        .context("Failed to create worktree base directory")?;

    let output = Command::new("git")
        .args(["worktree", "add", &worktree_path.to_string_lossy()])
        .output()
        .context("Failed to execute git worktree add")?;

    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow!("Failed to create worktree: {}", error));
    }

    let abs_path = fs::canonicalize(&worktree_path)
        .context("Failed to resolve worktree path")?;
    println!("{}", abs_path.display());
    Ok(())
}

fn get_existing_worktrees() -> Result<Vec<String>> {
    let output = Command::new("git")
        .args(["worktree", "list"])
        .output()
        .context("Failed to execute git worktree list")?;

    if !output.status.success() {
        return Err(anyhow!("Failed to list worktrees"));
    }

    let current_dir = std::env::current_dir()
        .context("Failed to get current directory")?;
    let current_worktree_name = current_dir
        .file_name()
        .and_then(|name| name.to_str())
        .unwrap_or("");

    let worktrees: Vec<String> = String::from_utf8_lossy(&output.stdout)
        .lines()
        .filter_map(|line| {
            let parts: Vec<&str> = line.split_whitespace().collect();
            if !parts.is_empty() {
                Path::new(parts[0])
                    .file_name()
                    .and_then(|name| name.to_str())
                    .map(|s| s.to_string())
            } else {
                None
            }
        })
        .filter(|name| name != "bare" && !name.is_empty() && name != current_worktree_name)
        .collect();

    Ok(worktrees)
}

fn clean_worktrees(pattern: Option<String>, force: bool, dry_run: bool, json: bool) -> Result<()> {
    let worktrees = get_existing_worktrees()?;

    if worktrees.is_empty() {
        handle_empty_result(json, dry_run, "No worktrees found.");
        return Ok(());
    }

    match pattern {
        Some(pattern) => clean_worktrees_by_pattern(&pattern, &worktrees, force, dry_run, json),
        None => {
            if dry_run {
                output_dry_run(&worktrees.iter().map(|s| s.as_str()).collect::<Vec<_>>(), json);
                Ok(())
            } else {
                clean_worktrees_interactive(&worktrees, force)
            }
        }
    }
}

fn clean_worktrees_by_pattern(
    pattern: &str,
    worktrees: &[String],
    force: bool,
    dry_run: bool,
    json: bool,
) -> Result<()> {
    let regex = Regex::new(pattern).map_err(|e| anyhow!("Invalid regex pattern: {}", e))?;

    let matching: Vec<&str> = worktrees
        .iter()
        .filter(|wt| regex.is_match(wt))
        .map(|s| s.as_str())
        .collect();

    if matching.is_empty() {
        handle_empty_result(json, dry_run, &format!("No worktrees matching '{}' found.", pattern));
        return Ok(());
    }

    if dry_run {
        output_dry_run(&matching, json);
        return Ok(());
    }

    display_items_for_deletion(&matching, "worktrees", &format!("matching '{}'", pattern));

    if !force && !confirm_deletion("worktrees", matching.len(), "local")? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_worktrees(&matching, force)
}

fn clean_worktrees_interactive(worktrees: &[String], force: bool) -> Result<()> {
    let items: Vec<&str> = worktrees.iter().map(|s| s.as_str()).collect();

    let selected = interactive_multi_select(
        &items,
        "Select worktrees to delete (space to toggle, enter to confirm, esc to cancel)",
        "worktrees",
    )?;

    let selected = match selected {
        Some(s) => s,
        None => return Ok(()),
    };

    display_items_for_deletion(&selected, "worktrees", "selected for deletion");

    if !force && !confirm_deletion("worktrees", selected.len(), "local")? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_worktrees(&selected, force)
}

fn clean_all_worktrees(force: bool, dry_run: bool, json: bool) -> Result<()> {
    let worktrees = get_existing_worktrees()?;

    if worktrees.is_empty() {
        handle_empty_result(json, dry_run, "No worktrees found.");
        return Ok(());
    }

    let refs: Vec<&str> = worktrees.iter().map(|s| s.as_str()).collect();

    if dry_run {
        output_dry_run(&refs, json);
        return Ok(());
    }

    println!(
        "{}",
        style("🔥 Preparing to remove all worktrees...").red().bold()
    );
    for wt in &worktrees {
        println!("  • {}", style(wt).yellow().bold());
    }

    if !force && !confirm_deletion("worktrees", worktrees.len(), "local")? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_worktrees(&refs, force)
}

fn delete_worktrees(worktrees: &[&str], force: bool) -> Result<()> {
    let mut deleted = 0;
    let mut failed = 0;

    println!();
    for name in worktrees {
        let path = match get_worktree_path(name) {
            Ok(p) => p,
            Err(e) => {
                print_deletion_result(&GitCommandResult::Failure(e.to_string()));
                failed += 1;
                continue;
            }
        };
        let path_str = path.to_string_lossy().to_string();

        print!(
            "{} {}... ",
            style("Deleting").red(),
            style(*name).yellow().bold()
        );

        if !path.exists() {
            print_deletion_result(&GitCommandResult::Failure("path not found".to_string()));
            failed += 1;
            continue;
        }

        let args: Vec<&str> = if force {
            vec!["worktree", "remove", "--force", &path_str]
        } else {
            vec!["worktree", "remove", &path_str]
        };

        let result = execute_git_command(&args);
        print_deletion_result(&result);
        match result {
            GitCommandResult::Success => deleted += 1,
            GitCommandResult::Failure(_) => failed += 1,
        }
    }

    print_deletion_summary(deleted, failed, "worktrees", "local");
    Ok(())
}

// ============================================================================
// Branch Functions
// ============================================================================

fn is_protected_branch(name: &str) -> bool {
    const DEFAULT_PROTECTED: &[&str] = &["main", "master", "develop", "development"];

    if DEFAULT_PROTECTED.contains(&name) {
        return true;
    }

    if let Ok(extra) = std::env::var("KILLALLGIT_PROTECTED") {
        return extra.split(',').map(|s| s.trim()).any(|s| s == name);
    }

    false
}

fn get_remote_branches(remote: &str) -> Result<Vec<String>> {
    let output = Command::new("git")
        .args(["branch", "-r"])
        .output()
        .context("Failed to execute git branch -r")?;

    if !output.status.success() {
        return Err(anyhow!("Failed to list remote branches"));
    }

    let prefix = format!("{}/", remote);
    let branches: Vec<String> = String::from_utf8_lossy(&output.stdout)
        .lines()
        .map(|line| line.trim())
        .filter(|line| line.starts_with(&prefix) && !line.contains("HEAD"))
        .map(|line| line.strip_prefix(&prefix).unwrap_or(line).to_string())
        .collect();

    Ok(branches)
}

fn get_current_branch() -> Result<String> {
    let output = Command::new("git")
        .args(["rev-parse", "--abbrev-ref", "HEAD"])
        .output()
        .context("Failed to execute git rev-parse --abbrev-ref HEAD")?;

    if !output.status.success() {
        return Err(anyhow!("Failed to get current branch"));
    }

    Ok(String::from_utf8_lossy(&output.stdout).trim().to_string())
}

fn get_local_branches() -> Result<Vec<String>> {
    let output = Command::new("git")
        .args(["branch", "--format=%(refname:short)"])
        .output()
        .context("Failed to execute git branch")?;

    if !output.status.success() {
        return Err(anyhow!("Failed to list local branches"));
    }

    let current = get_current_branch().unwrap_or_default();

    let branches: Vec<String> = String::from_utf8_lossy(&output.stdout)
        .lines()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty() && s != &current)
        .collect();

    Ok(branches)
}

fn clean_branches(
    pattern: Option<String>,
    force: bool,
    dry_run: bool,
    json: bool,
) -> Result<()> {
    match pattern {
        Some(pattern) => {
            let (scope, branch_pattern) = parse_branch_pattern(&pattern)?;
            match scope {
                BranchScope::Local => clean_local_branches_by_pattern(&branch_pattern, force, dry_run, json),
                BranchScope::Remote { remote } => clean_remote_branches_by_pattern(&branch_pattern, &remote, force, dry_run, json),
            }
        }
        None => {
            if dry_run {
                // For dry-run without pattern, default to showing local branches
                let branches = get_local_branches()?;
                let deletable: Vec<&str> = branches
                    .iter()
                    .map(|s| s.as_str())
                    .filter(|b| !is_protected_branch(b))
                    .collect();

                output_dry_run(&deletable, json);
                Ok(())
            } else {
                clean_branches_interactive(force)
            }
        }
    }
}

fn clean_local_branches_by_pattern(
    pattern: &str,
    force: bool,
    dry_run: bool,
    json: bool,
) -> Result<()> {
    let branches = get_local_branches()?;

    if branches.is_empty() {
        handle_empty_result(json, dry_run, "No local branches found.");
        return Ok(());
    }

    let regex = Regex::new(pattern).map_err(|e| anyhow!("Invalid regex pattern: {}", e))?;

    let matching: Vec<&str> = branches
        .iter()
        .filter(|b| regex.is_match(b))
        .map(|s| s.as_str())
        .collect();

    if matching.is_empty() {
        handle_empty_result(json, dry_run, &format!("No local branches matching '{}' found.", pattern));
        return Ok(());
    }

    let (protected, deletable): (Vec<&str>, Vec<&str>) =
        matching.into_iter().partition(|b| is_protected_branch(b));

    if dry_run {
        output_dry_run(&deletable, json);
        return Ok(());
    }

    display_protected_items(&protected, "branches");

    if deletable.is_empty() {
        println!(
            "{}",
            style("No deletable branches (all matched are protected).").yellow()
        );
        return Ok(());
    }

    display_items_for_deletion(&deletable, "local branches", &format!("matching '{}'", pattern));

    if !force && !confirm_deletion("branches", deletable.len(), "local")? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_local_branches(&deletable, force)
}

fn clean_remote_branches_by_pattern(
    pattern: &str,
    remote: &str,
    force: bool,
    dry_run: bool,
    json: bool,
) -> Result<()> {
    let branches = get_remote_branches(remote)?;

    if branches.is_empty() {
        handle_empty_result(json, dry_run, &format!("No remote branches found on '{}'.", remote));
        return Ok(());
    }

    let regex = Regex::new(pattern).map_err(|e| anyhow!("Invalid regex pattern: {}", e))?;

    let matching: Vec<&str> = branches
        .iter()
        .filter(|b| regex.is_match(b))
        .map(|s| s.as_str())
        .collect();

    if matching.is_empty() {
        handle_empty_result(json, dry_run, &format!("No remote branches matching '{}' found on '{}'.", pattern, remote));
        return Ok(());
    }

    let (protected, deletable): (Vec<&str>, Vec<&str>) =
        matching.into_iter().partition(|b| is_protected_branch(b));

    if dry_run {
        output_dry_run(&deletable, json);
        return Ok(());
    }

    display_protected_items(&protected, "branches");

    if deletable.is_empty() {
        println!(
            "{}",
            style("No deletable branches (all matched are protected).").yellow()
        );
        return Ok(());
    }

    display_items_for_deletion(&deletable, "remote branches", &format!("matching '{}' on '{}'", pattern, remote));

    if !force && !confirm_deletion("branches", deletable.len(), remote)? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_remote_branches(&deletable, remote)
}

fn clean_branches_interactive(force: bool) -> Result<()> {
    // First, ask user to select scope (local or remote)
    let remotes = get_configured_remotes()?;

    let mut scope_options = vec![style("Local branches").cyan().to_string()];
    for remote in &remotes {
        scope_options.push(style(format!("Remote: {}", remote)).cyan().to_string());
    }

    let scope_selection = Select::new()
        .with_prompt(style("Which branches do you want to clean?").cyan().to_string())
        .items(&scope_options)
        .interact_opt()?;

    let scope_selection = match scope_selection {
        Some(s) => s,
        None => {
            println!("{}", style("Selection cancelled.").yellow());
            return Ok(());
        }
    };

    if scope_selection == 0 {
        clean_local_branches_interactive(force)
    } else {
        let remote = &remotes[scope_selection - 1];
        clean_remote_branches_interactive(remote, force)
    }
}

fn clean_local_branches_interactive(force: bool) -> Result<()> {
    let branches = get_local_branches()?;

    let (protected, deletable): (Vec<&str>, Vec<&str>) = branches
        .iter()
        .map(|s| s.as_str())
        .partition(|b| is_protected_branch(b));

    display_protected_items(&protected, "branches (not selectable)");

    if deletable.is_empty() {
        println!(
            "{}",
            style("No deletable local branches (all are protected).").yellow()
        );
        return Ok(());
    }

    let selected = interactive_multi_select(
        &deletable,
        "Select local branches to delete (space to toggle, enter to confirm, esc to cancel)",
        "branches",
    )?;

    let selected = match selected {
        Some(s) => s,
        None => return Ok(()),
    };

    display_items_for_deletion(&selected, "local branches", "selected for deletion");

    if !force && !confirm_deletion("branches", selected.len(), "local")? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_local_branches(&selected, force)
}

fn clean_remote_branches_interactive(remote: &str, force: bool) -> Result<()> {
    let branches = get_remote_branches(remote)?;

    let (protected, deletable): (Vec<&str>, Vec<&str>) = branches
        .iter()
        .map(|s| s.as_str())
        .partition(|b| is_protected_branch(b));

    display_protected_items(&protected, "branches (not selectable)");

    if deletable.is_empty() {
        println!(
            "{}",
            style(format!("No deletable remote branches on '{}' (all are protected).", remote)).yellow()
        );
        return Ok(());
    }

    let selected = interactive_multi_select(
        &deletable,
        &format!("Select branches to delete from '{}' (space to toggle, enter to confirm, esc to cancel)", remote),
        "branches",
    )?;

    let selected = match selected {
        Some(s) => s,
        None => return Ok(()),
    };

    display_items_for_deletion(&selected, "remote branches", "selected for deletion");

    if !force && !confirm_deletion("branches", selected.len(), remote)? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_remote_branches(&selected, remote)
}

fn delete_local_branches(branches: &[&str], force: bool) -> Result<()> {
    let mut deleted = 0;
    let mut failed = 0;
    let flag = if force { "-D" } else { "-d" };

    println!();
    for branch in branches {
        print!(
            "{} {}... ",
            style("Deleting").red(),
            style(*branch).yellow().bold()
        );

        let result = execute_git_command(&["branch", flag, branch]);
        print_deletion_result(&result);
        match result {
            GitCommandResult::Success => deleted += 1,
            GitCommandResult::Failure(_) => failed += 1,
        }
    }

    print_deletion_summary(deleted, failed, "local branches", "local");
    Ok(())
}

fn delete_remote_branches(branches: &[&str], remote: &str) -> Result<()> {
    let mut deleted = 0;
    let mut failed = 0;

    println!();
    for branch in branches {
        print!(
            "{} {}... ",
            style("Deleting").red(),
            style(*branch).yellow().bold()
        );

        let result = execute_git_command(&["push", remote, "--delete", branch]);
        print_deletion_result(&result);
        match result {
            GitCommandResult::Success => deleted += 1,
            GitCommandResult::Failure(_) => failed += 1,
        }
    }

    print_deletion_summary(deleted, failed, "branches", remote);
    Ok(())
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn verify_cli() {
        use clap::CommandFactory;
        Cli::command().debug_assert();
    }

    #[test]
    fn test_add_worktree_cli() {
        use clap::Parser;

        let cli = Cli::try_parse_from(["killallgit", "add", "worktree", "feature-x"]).unwrap();
        match cli.command {
            Some(Commands::Add { resource: AddResource::Worktree { name } }) => {
                assert_eq!(name, "feature-x");
            }
            _ => panic!("Expected Add Worktree command"),
        }
    }

    #[test]
    fn test_clean_worktrees_cli() {
        use clap::Parser;

        // Without pattern
        let cli = Cli::try_parse_from(["killallgit", "clean", "worktrees"]).unwrap();
        match cli.command {
            Some(Commands::Clean {
                resource: CleanResource::Worktrees { pattern, force, all, dry_run, json },
            }) => {
                assert_eq!(pattern, None);
                assert!(!force);
                assert!(!all);
                assert!(!dry_run);
                assert!(!json);
            }
            _ => panic!("Expected Clean Worktrees command"),
        }

        // With pattern and flags
        let cli = Cli::try_parse_from([
            "killallgit", "clean", "worktrees", "feature/.*", "--force", "--dry-run", "--json"
        ]).unwrap();
        match cli.command {
            Some(Commands::Clean {
                resource: CleanResource::Worktrees { pattern, force, all, dry_run, json },
            }) => {
                assert_eq!(pattern, Some("feature/.*".to_string()));
                assert!(force);
                assert!(!all);
                assert!(dry_run);
                assert!(json);
            }
            _ => panic!("Expected Clean Worktrees command"),
        }

        // With --all
        let cli = Cli::try_parse_from(["killallgit", "clean", "worktrees", "--all"]).unwrap();
        match cli.command {
            Some(Commands::Clean {
                resource: CleanResource::Worktrees { pattern, all, .. },
            }) => {
                assert_eq!(pattern, None);
                assert!(all);
            }
            _ => panic!("Expected Clean Worktrees command"),
        }
    }

    #[test]
    fn test_clean_branches_cli() {
        use clap::Parser;

        // Without pattern
        let cli = Cli::try_parse_from(["killallgit", "clean", "branches"]).unwrap();
        match cli.command {
            Some(Commands::Clean {
                resource: CleanResource::Branches { pattern, force, dry_run, json },
            }) => {
                assert_eq!(pattern, None);
                assert!(!force);
                assert!(!dry_run);
                assert!(!json);
            }
            _ => panic!("Expected Clean Branches command"),
        }

        // With local pattern
        let cli = Cli::try_parse_from([
            "killallgit", "clean", "branches", "feature/.*", "--force"
        ]).unwrap();
        match cli.command {
            Some(Commands::Clean {
                resource: CleanResource::Branches { pattern, force, dry_run, json },
            }) => {
                assert_eq!(pattern, Some("feature/.*".to_string()));
                assert!(force);
                assert!(!dry_run);
                assert!(!json);
            }
            _ => panic!("Expected Clean Branches command"),
        }

        // With remote pattern (origin/ prefix)
        let cli = Cli::try_parse_from([
            "killallgit", "clean", "branches", "origin/feature/.*", "--force"
        ]).unwrap();
        match cli.command {
            Some(Commands::Clean {
                resource: CleanResource::Branches { pattern, force, .. },
            }) => {
                assert_eq!(pattern, Some("origin/feature/.*".to_string()));
                assert!(force);
            }
            _ => panic!("Expected Clean Branches command"),
        }

        // With --dry-run and --json
        let cli = Cli::try_parse_from([
            "killallgit", "clean", "branches", "--dry-run", "--json"
        ]).unwrap();
        match cli.command {
            Some(Commands::Clean {
                resource: CleanResource::Branches { dry_run, json, .. },
            }) => {
                assert!(dry_run);
                assert!(json);
            }
            _ => panic!("Expected Clean Branches command"),
        }
    }

    // Note: get_worktree_path() now requires git context and is tested via integration tests

    #[test]
    fn test_is_protected_branch() {
        assert!(is_protected_branch("main"));
        assert!(is_protected_branch("master"));
        assert!(is_protected_branch("develop"));
        assert!(is_protected_branch("development"));

        assert!(!is_protected_branch("feature/test"));
        assert!(!is_protected_branch("bugfix/fix-123"));
        assert!(!is_protected_branch("main-feature"));
    }

    #[test]
    fn test_is_protected_branch_with_env() {
        // SAFETY: This test runs in isolation; env var manipulation is safe here
        unsafe {
            std::env::set_var("KILLALLGIT_PROTECTED", "release, staging");
        }
        assert!(is_protected_branch("release"));
        assert!(is_protected_branch("staging"));
        assert!(is_protected_branch("main")); // defaults still protected
        assert!(!is_protected_branch("feature/test"));
        // SAFETY: Cleanup after test
        unsafe {
            std::env::remove_var("KILLALLGIT_PROTECTED");
        }
    }
}
