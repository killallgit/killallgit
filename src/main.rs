use anyhow::{anyhow, Result};
use clap::{Parser, Subcommand};
use dialoguer::{console::style, Select, FuzzySelect};
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
        },
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
}