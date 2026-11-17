// Synthetic local artifact protocol fixture, not engine behavior.
package artifacts

import (
    "net/http"
    "net/url"
    "strings"
    "testing"
    "github.com/nektos/act/pkg/common"
    "github.com/stretchr/testify/assert"
    "github.com/stretchr/testify/require"
    "google.golang.org/protobuf/encoding/protojson"
)

func TestOpenBF2ArtifactMetadataAndSignature(t *testing.T) {
    srv, _, authKey := newTestServer(t)
    token, err := common.CreateSignedAuthorizationToken(authKey, 7, 7, 7)
    require.NoError(t, err)
    status, body := doRequest(t, http.MethodPost, srv.URL+ArtifactV4RouteBase+"/CreateArtifact", token,
        `{"workflow_run_backend_id":"7","workflow_job_run_backend_id":"7","name":"v7","version":7,"mime_type":"application/zip"}`)
    require.Equal(t, http.StatusOK, status, body)
    created := CreateArtifactResponse{}
    require.NoError(t, protojson.Unmarshal([]byte(body), &created))
    upload, err := url.Parse(created.SignedUploadUrl)
    require.NoError(t, err)
    query := upload.Query()
    query.Set("sig", strings.TrimRight(query.Get("sig"), "="))
    query.Set("comp", "block")
    upload.RawQuery = query.Encode()
    status, body = doRequest(t, http.MethodPut, upload.String(), "", "zipdata")
    require.Equal(t, http.StatusCreated, status, body)
    query.Set("artifactName", "forged")
    upload.RawQuery = query.Encode()
    status, _ = doRequest(t, http.MethodPut, upload.String(), "", "invalid")
    assert.Equal(t, http.StatusUnauthorized, status)
    status, body = doRequest(t, http.MethodPost, srv.URL+ArtifactV4RouteBase+"/GetSignedArtifactURL", token,
        `{"workflow_run_backend_id":"7","workflow_job_run_backend_id":"7","name":"v7"}`)
    require.Equal(t, http.StatusOK, status, body)
    signed := GetSignedArtifactURLResponse{}
    require.NoError(t, protojson.Unmarshal([]byte(body), &signed))
    download, err := url.Parse(signed.SignedUrl)
    require.NoError(t, err)
    query = download.Query()
    query.Set("sig", strings.TrimRight(query.Get("sig"), "="))
    download.RawQuery = query.Encode()
    status, body = doRequest(t, http.MethodGet, download.String(), "", "")
    require.Equal(t, http.StatusOK, status, body)
    assert.Equal(t, "zipdata", body)
}
