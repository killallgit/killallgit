use anyhow::{anyhow, bail, Context, Result};
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
    /// Name of worktree to create
    name: Option<String>,

    #[command(subcommand)]
    command: Option<Commands>,
}

#[derive(Subcommand)]
enum Commands {
    /// Manage worktrees - list, create, or delete
    Worktree,
    /// Alias for 'worktree'
    Worktrees,
    /// Clean up worktrees or branches
    Clean {
        #[command(subcommand)]
        resource: Option<CleanResource>,
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

    match (cli.name, cli.command) {
        (Some(name), None) => add_worktree(name),
        (None, Some(Commands::Worktree | Commands::Worktrees)) => worktree_dashboard(),
        (None, Some(Commands::Clean { resource: Some(resource) })) => match resource {
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
        (None, Some(Commands::Clean { resource: None })) => clean_interactive_menu(),
        (None, None) => worktree_dashboard(),
        (Some(_), Some(_)) => Err(anyhow!(
            "Cannot specify both a worktree name and a subcommand.\n\
             Run 'killallgit --help' for usage."
        )),
    }
}

fn worktree_dashboard() -> Result<()> {
    use dialoguer::Input;

    require_terminal()?;

    let worktrees = get_existing_worktrees()?;

    // If no worktrees exist, go straight to create flow
    if worktrees.is_empty() {
        let name: String = Input::new()
            .with_prompt(style("Worktree name").cyan().to_string())
            .interact_text()?;
        return add_worktree(name);
    }

    // Build items: "Create new" + existing worktrees
    let mut items = vec![style("➕ Create new worktree").cyan().to_string()];
    for wt in &worktrees {
        items.push(style(wt).yellow().bold().to_string());
    }

    let selections = MultiSelect::new()
        .with_prompt(
            style("Worktrees (space to select, enter to confirm)").bold().to_string(),
        )
        .items(&items)
        .interact_opt()?;

    let indices = match selections {
        Some(s) if !s.is_empty() => s,
        _ => return Ok(()),
    };

    let create = indices.contains(&0);
    let to_delete: Vec<&str> = indices
        .iter()
        .filter(|&&i| i > 0)
        .map(|&i| worktrees[i - 1].as_str())
        .collect();

    // Create first
    if create {
        let name: String = Input::new()
            .with_prompt(style("Worktree name").cyan().to_string())
            .interact_text()?;
        add_worktree(name)?;
    }

    // Then delete selected worktrees
    if !to_delete.is_empty() {
        display_items_for_deletion(&to_delete, "worktrees", "selected for deletion");

        if confirm_deletion("worktrees", to_delete.len(), "local")? {
            delete_worktrees(&to_delete, false)?;
        } else {
            println!("{}", style("Deletion cancelled.").yellow());
        }
    }

    Ok(())
}

fn clean_interactive_menu() -> Result<()> {
    require_terminal()?;

    let options = &[
        style("Worktrees").cyan().to_string(),
        style("Branches").cyan().to_string(),
    ];

    let selection = Select::new()
        .with_prompt(style("What would you like to clean?").bold().to_string())
        .items(options)
        .interact_opt()?;

    match selection {
        Some(0) => clean_worktrees(None, false, false, false),
        Some(1) => clean_branches(None, false, false, false),
        _ => Ok(()),
    }
}

// ============================================================================
// Shared Utilities
// ============================================================================

fn require_terminal() -> Result<()> {
    if !std::io::stdin().is_terminal() {
        bail!(
            "Interactive mode requires a terminal. Use command-line arguments instead.\n\
             Run 'killallgit --help' for available commands."
        );
    }
    Ok(())
}

fn get_git_root() -> Result<std::path::PathBuf> {
    let output = Command::new("git")
        .args(["rev-parse", "--show-toplevel"])
        .output()
        .context("Failed to execute git rev-parse --show-toplevel")?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        bail!("Not in a git repository: {}", stderr.trim());
    }

    let path = String::from_utf8_lossy(&output.stdout).trim().to_string();
    Ok(std::path::PathBuf::from(path))
}

fn get_worktree_base_path() -> Result<std::path::PathBuf> {
    let git_root = get_git_root()?;
    let repo_name = git_root
        .file_name()
        .and_then(|name| name.to_str())
        .ok_or_else(|| anyhow!("Could not determine repository name"))?;
    let parent = git_root
        .parent()
        .ok_or_else(|| anyhow!("Could not determine parent directory"))?;
    Ok(parent.join(".worktrees").join(repo_name))
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
        .interact_opt()?;

    Ok(confirmation == Some(1))
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
// Branch Pattern Parsing
// ============================================================================

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

fn parse_branch_pattern(pattern: &str) -> Result<(BranchTarget, String)> {
    let remotes = get_configured_remotes()?;

    for remote in remotes {
        let prefix = format!("{}/", remote);
        if pattern.starts_with(&prefix) {
            let branch_pattern = pattern.strip_prefix(&prefix).unwrap().to_string();
            return Ok((BranchTarget::Remote(remote), branch_pattern));
        }
    }

    Ok((BranchTarget::Local, pattern.to_string()))
}

// ============================================================================
// Worktree Functions
// ============================================================================

fn add_worktree(name: String) -> Result<()> {
    let base_path = get_worktree_base_path()?;
    let worktree_path = base_path.join(&name);

    fs::create_dir_all(&base_path)
        .context("Failed to create worktree base directory")?;

    let output = Command::new("git")
        .args(["worktree", "add", &worktree_path.to_string_lossy()])
        .output()
        .context("Failed to execute git worktree add")?;

    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        bail!("Failed to create worktree: {}", error);
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
        let stderr = String::from_utf8_lossy(&output.stderr);
        bail!("Failed to list worktrees: {}", stderr.trim());
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
    let regex = Regex::new(pattern).context("Invalid regex pattern")?;

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
    let base_path = get_worktree_base_path()?;
    let mut deleted = 0;
    let mut failed = 0;

    println!();
    for name in worktrees {
        let path = base_path.join(name);
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

/// Represents a branch deletion target (local or remote)
#[derive(Clone)]
enum BranchTarget {
    Local,
    Remote(String),
}

impl BranchTarget {
    fn label(&self) -> &str {
        match self {
            BranchTarget::Local => "local",
            BranchTarget::Remote(r) => r,
        }
    }

    fn branch_type(&self) -> &str {
        match self {
            BranchTarget::Local => "local branches",
            BranchTarget::Remote(_) => "remote branches",
        }
    }

    fn get_branches(&self) -> Result<Vec<String>> {
        match self {
            BranchTarget::Local => get_local_branches(),
            BranchTarget::Remote(remote) => get_remote_branches(remote),
        }
    }

    fn delete_branch(&self, branch: &str, force: bool) -> GitCommandResult {
        match self {
            BranchTarget::Local => {
                let flag = if force { "-D" } else { "-d" };
                execute_git_command(&["branch", flag, branch])
            }
            BranchTarget::Remote(remote) => {
                execute_git_command(&["push", remote, "--delete", branch])
            }
        }
    }
}

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
        let stderr = String::from_utf8_lossy(&output.stderr);
        bail!("Failed to list remote branches: {}", stderr.trim());
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
        let stderr = String::from_utf8_lossy(&output.stderr);
        bail!("Failed to get current branch: {}", stderr.trim());
    }

    Ok(String::from_utf8_lossy(&output.stdout).trim().to_string())
}

fn get_local_branches() -> Result<Vec<String>> {
    let output = Command::new("git")
        .args(["branch", "--format=%(refname:short)"])
        .output()
        .context("Failed to execute git branch")?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        bail!("Failed to list local branches: {}", stderr.trim());
    }

    let current = get_current_branch()?;

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
            let (target, branch_pattern) = parse_branch_pattern(&pattern)?;
            clean_branches_by_pattern(&target, &branch_pattern, force, dry_run, json)
        }
        None => {
            if dry_run {
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

fn clean_branches_by_pattern(
    target: &BranchTarget,
    pattern: &str,
    force: bool,
    dry_run: bool,
    json: bool,
) -> Result<()> {
    let branches = target.get_branches()?;

    if branches.is_empty() {
        let msg = match target {
            BranchTarget::Local => "No local branches found.".to_string(),
            BranchTarget::Remote(r) => format!("No remote branches found on '{}'.", r),
        };
        handle_empty_result(json, dry_run, &msg);
        return Ok(());
    }

    let regex = Regex::new(pattern).context("Invalid regex pattern")?;

    let matching: Vec<&str> = branches
        .iter()
        .filter(|b| regex.is_match(b))
        .map(|s| s.as_str())
        .collect();

    if matching.is_empty() {
        let msg = match target {
            BranchTarget::Local => format!("No local branches matching '{}' found.", pattern),
            BranchTarget::Remote(r) => format!("No remote branches matching '{}' found on '{}'.", pattern, r),
        };
        handle_empty_result(json, dry_run, &msg);
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
        println!("{}", style("No deletable branches (all matched are protected).").yellow());
        return Ok(());
    }

    let context = match target {
        BranchTarget::Local => format!("matching '{}'", pattern),
        BranchTarget::Remote(r) => format!("matching '{}' on '{}'", pattern, r),
    };
    display_items_for_deletion(&deletable, target.branch_type(), &context);

    if !force && !confirm_deletion("branches", deletable.len(), target.label())? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_branches(target, &deletable, force)
}

fn clean_branches_interactive(force: bool) -> Result<()> {
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

    let target = if scope_selection == 0 {
        BranchTarget::Local
    } else {
        BranchTarget::Remote(remotes[scope_selection - 1].clone())
    };

    clean_branches_interactive_for_target(&target, force)
}

fn clean_branches_interactive_for_target(target: &BranchTarget, force: bool) -> Result<()> {
    let branches = target.get_branches()?;

    let (protected, deletable): (Vec<&str>, Vec<&str>) = branches
        .iter()
        .map(|s| s.as_str())
        .partition(|b| is_protected_branch(b));

    display_protected_items(&protected, "branches (not selectable)");

    if deletable.is_empty() {
        let msg = match target {
            BranchTarget::Local => "No deletable local branches (all are protected).".to_string(),
            BranchTarget::Remote(r) => format!("No deletable remote branches on '{}' (all are protected).", r),
        };
        println!("{}", style(msg).yellow());
        return Ok(());
    }

    let prompt = match target {
        BranchTarget::Local => "Select local branches to delete (space to toggle, enter to confirm, esc to cancel)".to_string(),
        BranchTarget::Remote(r) => format!("Select branches to delete from '{}' (space to toggle, enter to confirm, esc to cancel)", r),
    };

    let selected = interactive_multi_select(&deletable, &prompt, "branches")?;

    let selected = match selected {
        Some(s) => s,
        None => return Ok(()),
    };

    display_items_for_deletion(&selected, target.branch_type(), "selected for deletion");

    if !force && !confirm_deletion("branches", selected.len(), target.label())? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_branches(target, &selected, force)
}

fn delete_branches(target: &BranchTarget, branches: &[&str], force: bool) -> Result<()> {
    let mut deleted = 0;
    let mut failed = 0;

    println!();
    for branch in branches {
        print!(
            "{} {}... ",
            style("Deleting").red(),
            style(*branch).yellow().bold()
        );

        let result = target.delete_branch(branch, force);
        print_deletion_result(&result);
        match result {
            GitCommandResult::Success => deleted += 1,
            GitCommandResult::Failure(_) => failed += 1,
        }
    }

    print_deletion_summary(deleted, failed, target.branch_type(), target.label());
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

        let cli = Cli::try_parse_from(["killallgit", "feature-x"]).unwrap();
        assert_eq!(cli.name, Some("feature-x".to_string()));
        assert!(cli.command.is_none());
    }

    #[test]
    fn test_worktree_subcommand_cli() {
        use clap::Parser;

        let cli = Cli::try_parse_from(["killallgit", "worktree"]).unwrap();
        assert!(cli.name.is_none());
        assert!(matches!(cli.command, Some(Commands::Worktree)));

        let cli = Cli::try_parse_from(["killallgit", "worktrees"]).unwrap();
        assert!(cli.name.is_none());
        assert!(matches!(cli.command, Some(Commands::Worktrees)));
    }

    #[test]
    fn test_clean_no_subresource_cli() {
        use clap::Parser;

        let cli = Cli::try_parse_from(["killallgit", "clean"]).unwrap();
        match cli.command {
            Some(Commands::Clean { resource: None }) => {}
            _ => panic!("Expected Clean with no resource"),
        }
    }

    #[test]
    fn test_clean_worktrees_cli() {
        use clap::Parser;

        // Without pattern
        let cli = Cli::try_parse_from(["killallgit", "clean", "worktrees"]).unwrap();
        match cli.command {
            Some(Commands::Clean {
                resource: Some(CleanResource::Worktrees { pattern, force, all, dry_run, json }),
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
                resource: Some(CleanResource::Worktrees { pattern, force, all, dry_run, json }),
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
                resource: Some(CleanResource::Worktrees { pattern, all, .. }),
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
                resource: Some(CleanResource::Branches { pattern, force, dry_run, json }),
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
                resource: Some(CleanResource::Branches { pattern, force, dry_run, json }),
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
                resource: Some(CleanResource::Branches { pattern, force, .. }),
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
                resource: Some(CleanResource::Branches { dry_run, json, .. }),
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
        // SAFETY: Single-threaded test; no other code reads this env var concurrently
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
