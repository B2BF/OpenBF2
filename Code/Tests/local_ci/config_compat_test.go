// Synthetic local-runner compatibility fixture, not engine behavior.
package cmd

import (
    "os"
    "path/filepath"
    "testing"
    "github.com/spf13/cobra"
    "github.com/stretchr/testify/assert"
)

func TestOpenBF2ExplicitConfiguration(t *testing.T) {
    t.Setenv("OPENBF2_ACT_NO_CONFIG", "1")
    before := os.Args
    defer func() { os.Args = before }()
    beforeDirectory, err := os.Getwd()
    assert.NoError(t, err)
    directory := t.TempDir()
    assert.NoError(t, os.WriteFile(filepath.Join(directory, ".actrc"), []byte("--env UNEXPECTED=1\n"), 0600))
    assert.NoError(t, os.Chdir(directory))
    defer func() { _ = os.Chdir(beforeDirectory) }()
    os.Args = []string{"act", "workflow_dispatch", "--secret-file", "/dev/null"}
    assert.Equal(t, os.Args[1:], args(&cobra.Command{}))
}
