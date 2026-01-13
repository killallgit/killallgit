use anyhow::{anyhow, Result};
use clap::{Parser, Subcommand};
use dialoguer::{console::style, FuzzySelect, MultiSelect, Select};
use regex::Regex;
use std::fs;
use std::path::Path;
use std::process::Command;

#[derive(Parser)]
#[command(name = "killallgit")]
#[command(about = "A CLI tool for managing git worktrees")]
#[command(version)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Add a new worktree
    Add {
        /// Name of the worktree
        name: String,
    },
    /// Remove a worktree
    Remove {
        /// Name of the worktree (optional, will show selection list if not provided)
        name: Option<String>,
        /// Force removal even if there are uncommitted changes
        #[arg(long)]
        force: bool,
        /// Remove all worktrees (with confirmation)
        #[arg(long)]
        all: bool,
    },
    /// Clean up remote branches
    Clean {
        /// Regex pattern to match branch names (optional, interactive if not provided)
        pattern: Option<String>,
        /// Target remote (default: origin)
        #[arg(long, default_value = "origin")]
        remote: String,
        /// Skip confirmation prompts
        #[arg(long)]
        force: bool,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    match cli.command {
        Commands::Add { name } => add_worktree(name),
        Commands::Remove { name, force, all } => {
            if all {
                nuke_all_worktrees(force)
            } else {
                remove_worktree(name, force)
            }
        }
        Commands::Clean {
            pattern,
            remote,
            force,
        } => clean_remote_branches(pattern, &remote, force),
    }
}

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

fn add_worktree(name: String) -> Result<()> {
    let repo_name = get_repo_name()?;
    let worktree_path = get_worktree_path(&repo_name, &name);
    
    // Create the worktrees directory if it doesn't exist
    fs::create_dir_all(format!("../.worktrees/{}", repo_name))?;
    
    println!("{} '{}' {} {}", 
        style("Creating worktree").green(), 
        style(&name).yellow().bold(), 
        style("at:").green(), 
        style(&worktree_path).cyan());
    
    let output = Command::new("git")
        .args(["worktree", "add", &worktree_path])
        .output()?;
    
    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow!("Failed to create worktree: {}", error));
    }
    
    println!("{}", style("✓ Worktree created successfully!").green().bold());
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
                let path = parts[0];
                Path::new(path)
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

fn remove_worktree(name: Option<String>, force: bool) -> Result<()> {
    let worktrees = get_existing_worktrees()?;
    
    if worktrees.is_empty() {
        println!("{}", style("No worktrees found to remove.").red());
        return Ok(());
    }

    let selected_name = if let Some(name) = name {
        if !worktrees.contains(&name) {
            return Err(anyhow!("Worktree '{}' not found", name));
        }
        name
    } else if worktrees.len() == 1 {
        let mut items = vec![style(&worktrees[0]).yellow().bold().to_string()];
        items.push(style("🔥 Nuke all worktrees".to_string()).red().bold().to_string());
        
        let selection = Select::new()
            .with_prompt(style("Select a worktree to remove:").cyan().to_string())
            .items(&items)
            .interact()?;
        
        if selection == 1 {
            return nuke_all_worktrees(force);
        }
        
        worktrees[0].clone()
    } else {
        let mut styled_items: Vec<String> = worktrees.iter()
            .map(|name| style(name).yellow().bold().to_string())
            .collect();
        styled_items.push(style("🔥 Nuke all worktrees".to_string()).red().bold().to_string());
        
        let selection = FuzzySelect::new()
            .with_prompt(style("Select a worktree to remove (start typing to filter):").cyan().to_string())
            .items(&styled_items)
            .interact()?;
        
        if selection == worktrees.len() {
            return nuke_all_worktrees(force);
        }
        
        worktrees[selection].clone()
    };

    let repo_name = get_repo_name()?;
    let worktree_path = get_worktree_path(&repo_name, &selected_name);
    
    if !Path::new(&worktree_path).exists() {
        return Err(anyhow!("Worktree path does not exist: {}", worktree_path));
    }

    println!("{} '{}' {} {}", 
        style("Removing worktree").red(), 
        style(&selected_name).yellow().bold(), 
        style("at:").red(), 
        style(&worktree_path).cyan());
    
    let mut args = vec!["worktree", "remove"];
    if force {
        args.push("--force");
    }
    args.push(&worktree_path);
    
    let output = Command::new("git").args(&args).output()?;
    
    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow!("Failed to remove worktree: {}", error));
    }
    
    println!("{}", style("✓ Worktree removed successfully!").green().bold());
    Ok(())
}

fn nuke_all_worktrees(force: bool) -> Result<()> {
    let worktrees = get_existing_worktrees()?;
    
    if worktrees.is_empty() {
        println!("{}", style("No worktrees found to nuke.").red());
        return Ok(());
    }

    println!("{}", style("🔥 Preparing to nuke all worktrees...").red().bold());
    println!("{}", style("Found worktrees:").yellow());
    for worktree in &worktrees {
        println!("  • {}", style(worktree).yellow().bold());
    }

    if !force {
        let confirmation = Select::new()
            .with_prompt(style("⚠️  Are you sure you want to remove ALL worktrees?").red().bold().to_string())
            .items(&[
                style("❌ No, cancel").red().to_string(),
                style("💥 Yes, nuke them all!").red().bold().to_string(),
            ])
            .interact()?;

        if confirmation != 1 {
            println!("{}", style("Operation cancelled.").yellow());
            return Ok(());
        }
    }

    let repo_name = get_repo_name()?;
    let mut removed_count = 0;
    let mut failed_count = 0;

    for worktree_name in &worktrees {
        let worktree_path = get_worktree_path(&repo_name, worktree_name);
        
        if !Path::new(&worktree_path).exists() {
            eprintln!("{} {}", style("Warning:").yellow(), style(format!("Worktree path does not exist: {}", worktree_path)).dim());
            continue;
        }

        println!("{} {}", style("Removing").red(), style(worktree_name).yellow().bold());
        
        let mut args = vec!["worktree", "remove"];
        if force {
            args.push("--force");
        }
        args.push(&worktree_path);
        
        let output = Command::new("git").args(&args).output();
        
        match output {
            Ok(output) if output.status.success() => {
                removed_count += 1;
                println!("  {}", style("✓").green());
            }
            Ok(output) => {
                failed_count += 1;
                let error = String::from_utf8_lossy(&output.stderr);
                eprintln!("  {} {}", style("✗").red(), error.trim());
            }
            Err(e) => {
                failed_count += 1;
                eprintln!("  {} {}", style("✗").red(), e);
            }
        }
    }

    println!();
    println!("{}", style("🎯 Nuke complete!").green().bold());
    println!("  {} {}", style("Removed:").green(), removed_count);
    if failed_count > 0 {
        println!("  {} {}", style("Failed:").red(), failed_count);
    }

    Ok(())
}

// === Remote Branch Cleaning Functions ===

const PROTECTED_BRANCHES: &[&str] = &["main", "master", "develop", "development"];

fn is_protected_branch(name: &str) -> bool {
    PROTECTED_BRANCHES.contains(&name)
}

fn get_remote_branches(remote: &str) -> Result<Vec<String>> {
    let output = Command::new("git")
        .args(["branch", "-r"])
        .output()?;

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

fn delete_remote_branch(remote: &str, branch: &str) -> Result<()> {
    let output = Command::new("git")
        .args(["push", remote, "--delete", branch])
        .output()?;

    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow!("Failed to delete branch: {}", error.trim()));
    }

    Ok(())
}

fn clean_remote_branches(pattern: Option<String>, remote: &str, force: bool) -> Result<()> {
    let branches = get_remote_branches(remote)?;

    if branches.is_empty() {
        println!(
            "{}",
            style(format!("No remote branches found on '{}'.", remote)).yellow()
        );
        return Ok(());
    }

    match pattern {
        Some(pattern) => clean_with_pattern(&pattern, &branches, remote, force),
        None => clean_interactive(&branches, remote, force),
    }
}

fn clean_with_pattern(pattern: &str, branches: &[String], remote: &str, force: bool) -> Result<()> {
    let regex = Regex::new(pattern).map_err(|e| anyhow!("Invalid regex pattern: {}", e))?;

    let matching: Vec<&str> = branches
        .iter()
        .filter(|b| regex.is_match(b))
        .map(|s| s.as_str())
        .collect();

    if matching.is_empty() {
        println!(
            "{}",
            style(format!("No branches matching '{}' found.", pattern)).yellow()
        );
        return Ok(());
    }

    // Check for protected branches
    let (protected, deletable): (Vec<&str>, Vec<&str>) =
        matching.into_iter().partition(|b| is_protected_branch(b));

    if !protected.is_empty() {
        println!(
            "{}",
            style("⚠️  The following protected branches will be skipped:").yellow()
        );
        for branch in &protected {
            println!(
                "  • {} {}",
                style(*branch).yellow().bold(),
                style("(protected)").dim()
            );
        }
        println!();
    }

    if deletable.is_empty() {
        println!(
            "{}",
            style("No deletable branches found (all matched branches are protected).").yellow()
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

    if !force {
        let confirmation = Select::new()
            .with_prompt(
                style(format!(
                    "⚠️  Delete these {} branches from {}?",
                    deletable.len(),
                    remote
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

        if confirmation != 1 {
            println!("{}", style("Operation cancelled.").yellow());
            return Ok(());
        }
    }

    delete_branches(&deletable, remote)
}

fn clean_interactive(branches: &[String], remote: &str, force: bool) -> Result<()> {
    // Separate protected and deletable branches
    let (protected, deletable): (Vec<&str>, Vec<&str>) = branches
        .iter()
        .map(|s| s.as_str())
        .partition(|b| is_protected_branch(b));

    // Show protected branches as non-selectable (dimmed)
    if !protected.is_empty() {
        println!(
            "{}",
            style("Protected branches (not selectable):").dim()
        );
        for branch in &protected {
            println!("  {} {}", style("•").dim(), style(*branch).dim());
        }
        println!();
    }

    if deletable.is_empty() {
        println!(
            "{}",
            style("No deletable branches found (all branches are protected).").yellow()
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

    let selected_branches: Vec<&str> = selections
        .iter()
        .map(|&i| deletable[i])
        .collect();

    println!(
        "{}",
        style(format!("Selected {} branches for deletion:", selected_branches.len())).cyan()
    );
    for branch in &selected_branches {
        println!("  • {}", style(*branch).yellow().bold());
    }

    if !force {
        let confirmation = Select::new()
            .with_prompt(
                style(format!(
                    "⚠️  Delete these {} branches from {}?",
                    selected_branches.len(),
                    remote
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

        if confirmation != 1 {
            println!("{}", style("Operation cancelled.").yellow());
            return Ok(());
        }
    }

    delete_branches(&selected_branches, remote)
}

fn delete_branches<S: AsRef<str>>(branches: &[S], remote: &str) -> Result<()> {
    let mut deleted_count = 0;
    let mut failed_count = 0;

    println!();
    for branch in branches {
        let branch_name = branch.as_ref();
        print!(
            "{} {}... ",
            style("Deleting").red(),
            style(branch_name).yellow().bold()
        );

        match delete_remote_branch(remote, branch_name) {
            Ok(()) => {
                println!("{}", style("✓").green());
                deleted_count += 1;
            }
            Err(e) => {
                println!("{} {}", style("✗").red(), style(e.to_string()).dim());
                failed_count += 1;
            }
        }
    }

    println!();
    println!(
        "{}",
        style(format!("✓ Deleted {} branches from {}", deleted_count, remote))
            .green()
            .bold()
    );
    if failed_count > 0 {
        println!(
            "{}",
            style(format!("✗ Failed to delete {} branches", failed_count)).red()
        );
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cli_parsing() {
        use clap::Parser;
        
        // Test add command
        let cli = Cli::try_parse_from(&["killallgit", "add", "test-branch"]).unwrap();
        match cli.command {
            Commands::Add { name } => {
                assert_eq!(name, "test-branch");
            }
            _ => panic!("Expected Add command"),
        }

        // Test remove command with name and force
        let cli = Cli::try_parse_from(&["killallgit", "remove", "test-branch", "--force"]).unwrap();
        match cli.command {
            Commands::Remove { name, force, all } => {
                assert_eq!(name, Some("test-branch".to_string()));
                assert!(force);
                assert!(!all);
            }
            _ => panic!("Expected Remove command"),
        }

        // Test remove command without name
        let cli = Cli::try_parse_from(&["killallgit", "remove"]).unwrap();
        match cli.command {
            Commands::Remove { name, force, all } => {
                assert_eq!(name, None);
                assert!(!force);
                assert!(!all);
            }
            _ => panic!("Expected Remove command"),
        }
    }

    #[test]
    fn test_worktree_path_construction() {
        // Test that worktree path follows the expected pattern
        let repo_name = "test-repo";
        let worktree_name = "feature-branch";
        let path = get_worktree_path(repo_name, worktree_name);

        // Verify the path structure
        assert!(path.contains("../.worktrees/"));
        assert!(path.contains(repo_name));
        assert!(path.contains(worktree_name));
        assert_eq!(path, "../.worktrees/test-repo/feature-branch");
    }

    #[test]
    fn test_clean_cli_parsing() {
        use clap::Parser;

        // Test clean command without pattern (interactive mode)
        let cli = Cli::try_parse_from(&["killallgit", "clean"]).unwrap();
        match cli.command {
            Commands::Clean {
                pattern,
                remote,
                force,
            } => {
                assert_eq!(pattern, None);
                assert_eq!(remote, "origin");
                assert!(!force);
            }
            _ => panic!("Expected Clean command"),
        }

        // Test clean command with pattern
        let cli = Cli::try_parse_from(&["killallgit", "clean", "feature/.*"]).unwrap();
        match cli.command {
            Commands::Clean {
                pattern,
                remote,
                force,
            } => {
                assert_eq!(pattern, Some("feature/.*".to_string()));
                assert_eq!(remote, "origin");
                assert!(!force);
            }
            _ => panic!("Expected Clean command"),
        }

        // Test clean command with custom remote
        let cli =
            Cli::try_parse_from(&["killallgit", "clean", "--remote", "upstream"]).unwrap();
        match cli.command {
            Commands::Clean {
                pattern,
                remote,
                force,
            } => {
                assert_eq!(pattern, None);
                assert_eq!(remote, "upstream");
                assert!(!force);
            }
            _ => panic!("Expected Clean command"),
        }

        // Test clean command with pattern, custom remote, and force
        let cli = Cli::try_parse_from(&[
            "killallgit",
            "clean",
            "bugfix/.*",
            "--remote",
            "upstream",
            "--force",
        ])
        .unwrap();
        match cli.command {
            Commands::Clean {
                pattern,
                remote,
                force,
            } => {
                assert_eq!(pattern, Some("bugfix/.*".to_string()));
                assert_eq!(remote, "upstream");
                assert!(force);
            }
            _ => panic!("Expected Clean command"),
        }
    }

    #[test]
    fn test_is_protected_branch() {
        // Protected branches
        assert!(is_protected_branch("main"));
        assert!(is_protected_branch("master"));
        assert!(is_protected_branch("develop"));
        assert!(is_protected_branch("development"));

        // Non-protected branches
        assert!(!is_protected_branch("feature/test"));
        assert!(!is_protected_branch("bugfix/fix-123"));
        assert!(!is_protected_branch("release/v1.0"));
        assert!(!is_protected_branch("main-feature")); // Not exact match
        assert!(!is_protected_branch("my-develop-branch"));
    }
}