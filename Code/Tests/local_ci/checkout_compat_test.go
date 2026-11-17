// Synthetic local-runner compatibility fixture, not engine behavior.
package runner

import (
    "testing"
    "github.com/nektos/act/pkg/model"
    "github.com/stretchr/testify/assert"
)

func TestOpenBF2FrozenCheckout(t *testing.T) {
    sha := "0123456789012345678901234567890123456789"
    context := &model.GithubContext{Repository: "owner/engine", Ref: "refs/heads/local-ci", Sha: sha}
    step := &model.Step{Uses: "actions/checkout@v7", With: map[string]string{"ref": "${{ github.sha }}"}}
    assert.False(t, isLocalCheckout(context, step))
    t.Setenv("OPENBF2_ACT_FROZEN_SHA", sha)
    assert.True(t, isLocalCheckout(context, step))
    step.With["ref"] = sha
    assert.True(t, isLocalCheckout(context, step))
    step.With["repository"] = "owner/setup"
    assert.False(t, isLocalCheckout(context, step))
    delete(step.With, "repository")
    step.With["ref"] = "other-commit"
    assert.False(t, isLocalCheckout(context, step))
    step.With["ref"] = "${{ github.sha }}"
    t.Setenv("OPENBF2_ACT_FROZEN_SHA", "different")
    assert.False(t, isLocalCheckout(context, step))
}
