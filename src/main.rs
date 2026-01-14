use anyhow::{anyhow, Result};
use clap::{Parser, Subcommand};
use dialoguer::{console::style, MultiSelect, Select};
use regex::Regex;
use std::fs;
use std::path::Path;
use std::process::Command;

#[derive(Parser)]
#[command(name = "killallgit")]
#[command(about = "A CLI tool for managing git worktrees and branches")]
#[command(version)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
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
        /// Output as JSON array (use with --dry-run)
        #[arg(long)]
        json: bool,
    },
    /// Clean up remote branches
    Branches {
        /// Regex pattern to match branch names (optional, interactive if not provided)
        pattern: Option<String>,
        /// Target remote (default: origin)
        #[arg(long, default_value = "origin")]
        remote: String,
        /// Skip confirmation prompts
        #[arg(long)]
        force: bool,
        /// List matching branches without deleting
        #[arg(long)]
        dry_run: bool,
        /// Output as JSON array (use with --dry-run)
        #[arg(long)]
        json: bool,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    match cli.command {
        Commands::Add { resource } => match resource {
            AddResource::Worktree { name } => add_worktree(name),
        },
        Commands::Clean { resource } => match resource {
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
                remote,
                force,
                dry_run,
                json,
            } => clean_branches(pattern, &remote, force, dry_run, json),
        },
    }
}

// ============================================================================
// Shared Utilities
// ============================================================================

fn get_worktree_path(repo_name: &str, worktree_name: &str) -> String {
    format!("../.worktrees/{}/{}", repo_name, worktree_name)
}

fn get_repo_name() -> Result<String> {
    let output = Command::new("git")
        .args(["rev-parse", "--show-toplevel"])
        .output()?;

    if !output.status.success() {
        return Err(anyhow!("Not in a git repository"));
    }

    let binding = String::from_utf8_lossy(&output.stdout);
    let repo_path = binding.trim();
    let repo_name = Path::new(repo_path)
        .file_name()
        .and_then(|name| name.to_str())
        .ok_or_else(|| anyhow!("Could not determine repository name"))?;

    Ok(repo_name.to_string())
}

fn print_json_array(items: &[&str]) {
    let escaped: Vec<String> = items
        .iter()
        .map(|s| format!("\"{}\"", s.replace('\\', "\\\\").replace('"', "\\\"")))
        .collect();
    println!("[{}]", escaped.join(","));
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

// ============================================================================
// Worktree Functions
// ============================================================================

fn add_worktree(name: String) -> Result<()> {
    let repo_name = get_repo_name()?;
    let worktree_path = get_worktree_path(&repo_name, &name);

    fs::create_dir_all(format!("../.worktrees/{}", repo_name))?;

    let output = Command::new("git")
        .args(["worktree", "add", &worktree_path])
        .output()?;

    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow!("Failed to create worktree: {}", error));
    }

    let abs_path = fs::canonicalize(&worktree_path)?;
    println!("{}", abs_path.display());
    Ok(())
}

fn get_existing_worktrees() -> Result<Vec<String>> {
    let output = Command::new("git")
        .args(["worktree", "list"])
        .output()?;

    if !output.status.success() {
        return Err(anyhow!("Failed to list worktrees"));
    }

    let current_dir = std::env::current_dir()?;
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
        if json {
            println!("[]");
        } else if !dry_run {
            println!("{}", style("No worktrees found.").yellow());
        }
        return Ok(());
    }

    match pattern {
        Some(pattern) => clean_worktrees_by_pattern(&pattern, &worktrees, force, dry_run, json),
        None => {
            if dry_run {
                if json {
                    print_json_array(&worktrees.iter().map(|s| s.as_str()).collect::<Vec<_>>());
                } else {
                    for wt in &worktrees {
                        println!("{}", wt);
                    }
                }
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
        if json {
            println!("[]");
        } else if !dry_run {
            println!(
                "{}",
                style(format!("No worktrees matching '{}' found.", pattern)).yellow()
            );
        }
        return Ok(());
    }

    if dry_run {
        if json {
            print_json_array(&matching);
        } else {
            for wt in &matching {
                println!("{}", wt);
            }
        }
        return Ok(());
    }

    println!(
        "{}",
        style(format!(
            "Found {} worktrees matching '{}':",
            matching.len(),
            pattern
        ))
        .cyan()
    );
    for wt in &matching {
        println!("  • {}", style(*wt).yellow().bold());
    }

    if !force && !confirm_deletion("worktrees", matching.len(), "local")? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_worktrees(&matching, force)
}

fn clean_worktrees_interactive(worktrees: &[String], force: bool) -> Result<()> {
    let styled_items: Vec<String> = worktrees
        .iter()
        .map(|name| style(name.as_str()).yellow().bold().to_string())
        .collect();

    let selections = MultiSelect::new()
        .with_prompt(
            style("Select worktrees to delete (space to toggle, enter to confirm)")
                .cyan()
                .to_string(),
        )
        .items(&styled_items)
        .interact()?;

    if selections.is_empty() {
        println!("{}", style("No worktrees selected.").yellow());
        return Ok(());
    }

    let selected: Vec<&str> = selections.iter().map(|&i| worktrees[i].as_str()).collect();

    println!(
        "{}",
        style(format!("Selected {} worktrees for deletion:", selected.len())).cyan()
    );
    for wt in &selected {
        println!("  • {}", style(*wt).yellow().bold());
    }

    if !force && !confirm_deletion("worktrees", selected.len(), "local")? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_worktrees(&selected, force)
}

fn clean_all_worktrees(force: bool, dry_run: bool, json: bool) -> Result<()> {
    let worktrees = get_existing_worktrees()?;

    if worktrees.is_empty() {
        if json {
            println!("[]");
        } else if !dry_run {
            println!("{}", style("No worktrees found.").yellow());
        }
        return Ok(());
    }

    if dry_run {
        if json {
            print_json_array(&worktrees.iter().map(|s| s.as_str()).collect::<Vec<_>>());
        } else {
            for wt in &worktrees {
                println!("{}", wt);
            }
        }
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

    let refs: Vec<&str> = worktrees.iter().map(|s| s.as_str()).collect();
    delete_worktrees(&refs, force)
}

fn delete_worktrees(worktrees: &[&str], force: bool) -> Result<()> {
    let repo_name = get_repo_name()?;
    let mut deleted = 0;
    let mut failed = 0;

    println!();
    for name in worktrees {
        let path = get_worktree_path(&repo_name, name);
        print!(
            "{} {}... ",
            style("Deleting").red(),
            style(*name).yellow().bold()
        );

        if !Path::new(&path).exists() {
            println!("{} {}", style("✗").red(), style("path not found").dim());
            failed += 1;
            continue;
        }

        let mut args = vec!["worktree", "remove"];
        if force {
            args.push("--force");
        }
        args.push(&path);

        match Command::new("git").args(&args).output() {
            Ok(out) if out.status.success() => {
                println!("{}", style("✓").green());
                deleted += 1;
            }
            Ok(out) => {
                let err = String::from_utf8_lossy(&out.stderr);
                println!("{} {}", style("✗").red(), style(err.trim()).dim());
                failed += 1;
            }
            Err(e) => {
                println!("{} {}", style("✗").red(), style(e.to_string()).dim());
                failed += 1;
            }
        }
    }

    println!();
    println!(
        "{}",
        style(format!("✓ Deleted {} worktrees", deleted)).green().bold()
    );
    if failed > 0 {
        println!(
            "{}",
            style(format!("✗ Failed to delete {} worktrees", failed)).red()
        );
    }

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
    let output = Command::new("git").args(["branch", "-r"]).output()?;

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

fn clean_branches(
    pattern: Option<String>,
    remote: &str,
    force: bool,
    dry_run: bool,
    json: bool,
) -> Result<()> {
    let branches = get_remote_branches(remote)?;

    if branches.is_empty() {
        if json {
            println!("[]");
        } else if !dry_run {
            println!(
                "{}",
                style(format!("No remote branches found on '{}'.", remote)).yellow()
            );
        }
        return Ok(());
    }

    match pattern {
        Some(pattern) => clean_branches_by_pattern(&pattern, &branches, remote, force, dry_run, json),
        None => {
            if dry_run {
                let deletable: Vec<&str> = branches
                    .iter()
                    .map(|s| s.as_str())
                    .filter(|b| !is_protected_branch(b))
                    .collect();

                if json {
                    print_json_array(&deletable);
                } else {
                    for branch in deletable {
                        println!("{}", branch);
                    }
                }
                Ok(())
            } else {
                clean_branches_interactive(&branches, remote, force)
            }
        }
    }
}

fn clean_branches_by_pattern(
    pattern: &str,
    branches: &[String],
    remote: &str,
    force: bool,
    dry_run: bool,
    json: bool,
) -> Result<()> {
    let regex = Regex::new(pattern).map_err(|e| anyhow!("Invalid regex pattern: {}", e))?;

    let matching: Vec<&str> = branches
        .iter()
        .filter(|b| regex.is_match(b))
        .map(|s| s.as_str())
        .collect();

    if matching.is_empty() {
        if json {
            println!("[]");
        } else if !dry_run {
            println!(
                "{}",
                style(format!("No branches matching '{}' found.", pattern)).yellow()
            );
        }
        return Ok(());
    }

    let (protected, deletable): (Vec<&str>, Vec<&str>) =
        matching.into_iter().partition(|b| is_protected_branch(b));

    if dry_run {
        if json {
            print_json_array(&deletable);
        } else {
            for branch in &deletable {
                println!("{}", branch);
            }
        }
        return Ok(());
    }

    if !protected.is_empty() {
        println!(
            "{}",
            style("⚠️  Protected branches (skipped):").yellow()
        );
        for branch in &protected {
            println!("  {} {}", style("•").dim(), style(*branch).dim());
        }
        println!();
    }

    if deletable.is_empty() {
        println!(
            "{}",
            style("No deletable branches (all matched are protected).").yellow()
        );
        return Ok(());
    }

    println!(
        "{}",
        style(format!(
            "Found {} branches matching '{}':",
            deletable.len(),
            pattern
        ))
        .cyan()
    );
    for branch in &deletable {
        println!("  • {}", style(*branch).yellow().bold());
    }

    if !force && !confirm_deletion("branches", deletable.len(), remote)? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_branches(&deletable, remote)
}

fn clean_branches_interactive(branches: &[String], remote: &str, force: bool) -> Result<()> {
    let (protected, deletable): (Vec<&str>, Vec<&str>) = branches
        .iter()
        .map(|s| s.as_str())
        .partition(|b| is_protected_branch(b));

    if !protected.is_empty() {
        println!("{}", style("Protected branches (not selectable):").dim());
        for branch in &protected {
            println!("  {} {}", style("•").dim(), style(*branch).dim());
        }
        println!();
    }

    if deletable.is_empty() {
        println!(
            "{}",
            style("No deletable branches (all are protected).").yellow()
        );
        return Ok(());
    }

    let styled_items: Vec<String> = deletable
        .iter()
        .map(|name| style(*name).yellow().bold().to_string())
        .collect();

    let selections = MultiSelect::new()
        .with_prompt(
            style("Select branches to delete (space to toggle, enter to confirm)")
                .cyan()
                .to_string(),
        )
        .items(&styled_items)
        .interact()?;

    if selections.is_empty() {
        println!("{}", style("No branches selected.").yellow());
        return Ok(());
    }

    let selected: Vec<&str> = selections.iter().map(|&i| deletable[i]).collect();

    println!(
        "{}",
        style(format!("Selected {} branches for deletion:", selected.len())).cyan()
    );
    for branch in &selected {
        println!("  • {}", style(*branch).yellow().bold());
    }

    if !force && !confirm_deletion("branches", selected.len(), remote)? {
        println!("{}", style("Operation cancelled.").yellow());
        return Ok(());
    }

    delete_branches(&selected, remote)
}

fn delete_branches(branches: &[&str], remote: &str) -> Result<()> {
    let mut deleted = 0;
    let mut failed = 0;

    println!();
    for branch in branches {
        print!(
            "{} {}... ",
            style("Deleting").red(),
            style(*branch).yellow().bold()
        );

        let output = Command::new("git")
            .args(["push", remote, "--delete", branch])
            .output();

        match output {
            Ok(out) if out.status.success() => {
                println!("{}", style("✓").green());
                deleted += 1;
            }
            Ok(out) => {
                let err = String::from_utf8_lossy(&out.stderr);
                println!("{} {}", style("✗").red(), style(err.trim()).dim());
                failed += 1;
            }
            Err(e) => {
                println!("{} {}", style("✗").red(), style(e.to_string()).dim());
                failed += 1;
            }
        }
    }

    println!();
    println!(
        "{}",
        style(format!("✓ Deleted {} branches from {}", deleted, remote))
            .green()
            .bold()
    );
    if failed > 0 {
        println!(
            "{}",
            style(format!("✗ Failed to delete {} branches", failed)).red()
        );
    }

    Ok(())
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add_worktree_cli() {
        use clap::Parser;

        let cli = Cli::try_parse_from(&["killallgit", "add", "worktree", "feature-x"]).unwrap();
        match cli.command {
            Commands::Add { resource: AddResource::Worktree { name } } => {
                assert_eq!(name, "feature-x");
            }
            _ => panic!("Expected Add Worktree command"),
        }
    }

    #[test]
    fn test_clean_worktrees_cli() {
        use clap::Parser;

        // Without pattern
        let cli = Cli::try_parse_from(&["killallgit", "clean", "worktrees"]).unwrap();
        match cli.command {
            Commands::Clean {
                resource: CleanResource::Worktrees { pattern, force, all, dry_run, json },
            } => {
                assert_eq!(pattern, None);
                assert!(!force);
                assert!(!all);
                assert!(!dry_run);
                assert!(!json);
            }
            _ => panic!("Expected Clean Worktrees command"),
        }

        // With pattern and flags
        let cli = Cli::try_parse_from(&[
            "killallgit", "clean", "worktrees", "feature/.*", "--force", "--dry-run", "--json"
        ]).unwrap();
        match cli.command {
            Commands::Clean {
                resource: CleanResource::Worktrees { pattern, force, all, dry_run, json },
            } => {
                assert_eq!(pattern, Some("feature/.*".to_string()));
                assert!(force);
                assert!(!all);
                assert!(dry_run);
                assert!(json);
            }
            _ => panic!("Expected Clean Worktrees command"),
        }

        // With --all
        let cli = Cli::try_parse_from(&["killallgit", "clean", "worktrees", "--all"]).unwrap();
        match cli.command {
            Commands::Clean {
                resource: CleanResource::Worktrees { pattern, all, .. },
            } => {
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
        let cli = Cli::try_parse_from(&["killallgit", "clean", "branches"]).unwrap();
        match cli.command {
            Commands::Clean {
                resource: CleanResource::Branches { pattern, remote, force, dry_run, json },
            } => {
                assert_eq!(pattern, None);
                assert_eq!(remote, "origin");
                assert!(!force);
                assert!(!dry_run);
                assert!(!json);
            }
            _ => panic!("Expected Clean Branches command"),
        }

        // With pattern and custom remote
        let cli = Cli::try_parse_from(&[
            "killallgit", "clean", "branches", "feature/.*", "--remote", "upstream", "--force"
        ]).unwrap();
        match cli.command {
            Commands::Clean {
                resource: CleanResource::Branches { pattern, remote, force, dry_run, json },
            } => {
                assert_eq!(pattern, Some("feature/.*".to_string()));
                assert_eq!(remote, "upstream");
                assert!(force);
                assert!(!dry_run);
                assert!(!json);
            }
            _ => panic!("Expected Clean Branches command"),
        }

        // With --dry-run and --json
        let cli = Cli::try_parse_from(&[
            "killallgit", "clean", "branches", "--dry-run", "--json"
        ]).unwrap();
        match cli.command {
            Commands::Clean {
                resource: CleanResource::Branches { dry_run, json, .. },
            } => {
                assert!(dry_run);
                assert!(json);
            }
            _ => panic!("Expected Clean Branches command"),
        }
    }

    #[test]
    fn test_worktree_path_construction() {
        let path = get_worktree_path("my-repo", "feature-branch");
        assert_eq!(path, "../.worktrees/my-repo/feature-branch");
    }

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
