use std::process::Command;
use std::path::PathBuf;
use std::fs;
use std::sync::atomic::{AtomicU64, Ordering};

static TEST_COUNTER: AtomicU64 = AtomicU64::new(0);

struct TestRepo {
    work_dir: PathBuf,
    temp_dir: PathBuf,
}

impl TestRepo {
    fn new() -> Self {
        let unique_id = TEST_COUNTER.fetch_add(1, Ordering::SeqCst);
        let temp_dir = std::env::temp_dir().join(format!(
            "killallgit_test_{}_{}",
            std::process::id(),
            unique_id
        ));
        let work_dir = temp_dir.join("work");
        let bare_dir = temp_dir.join("bare.git");

        // Clean up if exists
        let _ = fs::remove_dir_all(&temp_dir);
        fs::create_dir_all(&work_dir).unwrap();
        fs::create_dir_all(&bare_dir).unwrap();

        // Initialize bare repo
        Command::new("git")
            .args(["init", "--bare"])
            .current_dir(&bare_dir)
            .output()
            .expect("Failed to init bare repo");

        // Initialize work repo
        Command::new("git")
            .args(["init"])
            .current_dir(&work_dir)
            .output()
            .expect("Failed to init work repo");

        // Configure git user for commits
        Command::new("git")
            .args(["config", "user.email", "test@test.com"])
            .current_dir(&work_dir)
            .output()
            .expect("Failed to configure git email");

        Command::new("git")
            .args(["config", "user.name", "Test User"])
            .current_dir(&work_dir)
            .output()
            .expect("Failed to configure git name");

        // Add remote
        Command::new("git")
            .args(["remote", "add", "origin", bare_dir.to_str().unwrap()])
            .current_dir(&work_dir)
            .output()
            .expect("Failed to add remote");

        // Create initial commit
        fs::write(work_dir.join("README.md"), "# Test").unwrap();
        Command::new("git")
            .args(["add", "."])
            .current_dir(&work_dir)
            .output()
            .expect("Failed to add files");

        Command::new("git")
            .args(["commit", "-m", "Initial commit"])
            .current_dir(&work_dir)
            .output()
            .expect("Failed to commit");

        // Push main branch
        Command::new("git")
            .args(["push", "-u", "origin", "main"])
            .current_dir(&work_dir)
            .output()
            .expect("Failed to push main");

        TestRepo { work_dir, temp_dir }
    }

    fn create_branch(&self, name: &str) {
        Command::new("git")
            .args(["checkout", "-b", name])
            .current_dir(&self.work_dir)
            .output()
            .expect("Failed to create branch");

        Command::new("git")
            .args(["push", "-u", "origin", name])
            .current_dir(&self.work_dir)
            .output()
            .expect("Failed to push branch");

        // Go back to main
        Command::new("git")
            .args(["checkout", "main"])
            .current_dir(&self.work_dir)
            .output()
            .expect("Failed to checkout main");
    }

    fn get_remote_branches(&self) -> Vec<String> {
        let output = Command::new("git")
            .args(["branch", "-r"])
            .current_dir(&self.work_dir)
            .output()
            .expect("Failed to list branches");

        String::from_utf8_lossy(&output.stdout)
            .lines()
            .map(|l| l.trim().to_string())
            .filter(|l| !l.contains("HEAD"))
            .collect()
    }

    fn run_killallgit(&self, args: &[&str]) -> (String, String, bool) {
        // Get binary path from cargo
        let binary = option_env!("CARGO_BIN_EXE_killallgit")
            .map(PathBuf::from)
            .unwrap_or_else(|| {
                // Fallback: find binary relative to manifest dir
                let manifest_dir = env!("CARGO_MANIFEST_DIR");
                PathBuf::from(manifest_dir).join("target/debug/killallgit")
            });

        let output = Command::new(&binary)
            .args(args)
            .current_dir(&self.work_dir)
            .output()
            .unwrap_or_else(|e| panic!("Failed to run killallgit at {:?}: {}", binary, e));

        (
            String::from_utf8_lossy(&output.stdout).to_string(),
            String::from_utf8_lossy(&output.stderr).to_string(),
            output.status.success(),
        )
    }
}

impl Drop for TestRepo {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.temp_dir);
    }
}

#[test]
fn test_clean_branches_dry_run_lists_remote_branches() {
    let repo = TestRepo::new();

    // Create test branches
    repo.create_branch("feature/test-1");
    repo.create_branch("feature/test-2");
    repo.create_branch("bugfix/fix-123");

    // Run dry-run with origin/ prefix for remote branches
    let (stdout, _stderr, success) = repo.run_killallgit(&["clean", "branches", "origin/.*", "--dry-run"]);

    assert!(success, "Command should succeed");
    assert!(stdout.contains("feature/test-1"), "Should list feature/test-1");
    assert!(stdout.contains("feature/test-2"), "Should list feature/test-2");
    assert!(stdout.contains("bugfix/fix-123"), "Should list bugfix/fix-123");
    // main is protected, should not be listed
    assert!(!stdout.lines().any(|l| l.trim() == "main"), "Should not list main (protected)");
}

#[test]
fn test_clean_branches_dry_run_with_pattern() {
    let repo = TestRepo::new();

    // Create test branches
    repo.create_branch("feature/test-1");
    repo.create_branch("feature/test-2");
    repo.create_branch("bugfix/fix-123");

    // Run dry-run with pattern (origin/ prefix for remote branches)
    let (stdout, _stderr, success) = repo.run_killallgit(&["clean", "branches", "origin/feature/.*", "--dry-run"]);

    assert!(success, "Command should succeed");
    assert!(stdout.contains("feature/test-1"), "Should list feature/test-1");
    assert!(stdout.contains("feature/test-2"), "Should list feature/test-2");
    assert!(!stdout.contains("bugfix/fix-123"), "Should not list bugfix/fix-123");
}

#[test]
fn test_clean_branches_dry_run_json_output() {
    let repo = TestRepo::new();

    // Create test branches
    repo.create_branch("feature/json-test-1");
    repo.create_branch("feature/json-test-2");

    // Run dry-run with JSON (origin/ prefix for remote branches)
    let (stdout, _stderr, success) = repo.run_killallgit(&["clean", "branches", "origin/feature/json.*", "--dry-run", "--json"]);

    assert!(success, "Command should succeed");

    // Parse JSON
    let stdout_trimmed = stdout.trim();
    assert!(stdout_trimmed.starts_with('['), "Should be JSON array");
    assert!(stdout_trimmed.ends_with(']'), "Should be JSON array");
    assert!(stdout_trimmed.contains("feature/json-test-1"), "Should contain branch 1");
    assert!(stdout_trimmed.contains("feature/json-test-2"), "Should contain branch 2");
}

#[test]
fn test_clean_branches_actually_deletes() {
    let repo = TestRepo::new();

    // Create test branches
    repo.create_branch("feature/delete-me-1");
    repo.create_branch("feature/delete-me-2");

    // Verify branches exist
    let branches_before = repo.get_remote_branches();
    assert!(branches_before.iter().any(|b| b.contains("feature/delete-me-1")));
    assert!(branches_before.iter().any(|b| b.contains("feature/delete-me-2")));

    // Run clean with force (origin/ prefix for remote branches)
    let (stdout, _stderr, success) = repo.run_killallgit(&["clean", "branches", "origin/feature/delete-me.*", "--force"]);

    assert!(success, "Command should succeed");
    assert!(stdout.contains("Deleted"), "Should show deletion message");

    // Fetch to update remote refs
    Command::new("git")
        .args(["fetch", "--prune"])
        .current_dir(&repo.work_dir)
        .output()
        .expect("Failed to fetch");

    // Verify branches are gone
    let branches_after = repo.get_remote_branches();
    assert!(!branches_after.iter().any(|b| b.contains("feature/delete-me-1")), "Branch 1 should be deleted");
    assert!(!branches_after.iter().any(|b| b.contains("feature/delete-me-2")), "Branch 2 should be deleted");
}

#[test]
fn test_clean_branches_protects_main() {
    let repo = TestRepo::new();

    // Try to delete main with pattern that matches it (origin/ prefix for remote)
    let (stdout, _stderr, success) = repo.run_killallgit(&["clean", "branches", "origin/main", "--dry-run"]);

    assert!(success, "Command should succeed");
    // stdout should be empty or not contain main as deletable
    let lines: Vec<&str> = stdout.lines().filter(|l| !l.is_empty()).collect();
    assert!(lines.is_empty() || !lines.iter().any(|l| l.trim() == "main"),
        "Should not list main as deletable");
}

#[test]
fn test_clean_branches_no_matching() {
    let repo = TestRepo::new();

    // Run dry-run with pattern that matches nothing (origin/ prefix for remote)
    let (stdout, _stderr, success) = repo.run_killallgit(&["clean", "branches", "origin/nonexistent/.*", "--dry-run", "--json"]);

    assert!(success, "Command should succeed");
    assert_eq!(stdout.trim(), "[]", "Should return empty JSON array");
}
