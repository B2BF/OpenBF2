# Local act compatibility

These patches apply to the exact upstream revision in `runner-config.json`.
They implement the local runner discussed in [OpenBF2 #47](https://github.com/kr4uzi/OpenBF2/issues/47).

- Upload-artifact v7 sends optional protobuf fields and uses unpadded base64 URL
  signatures. Accept those encodings while keeping HMAC and expiry checks intact.
- `OPENBF2_ACT_NO_CONFIG=1` disables implicit `.actrc` files. The wrapper supplies
  every option explicitly, so user or repository configuration cannot silently
  inject credentials or change the tested job selection.
- `OPENBF2_ACT_FROZEN_SHA` enables local checkout substitution for that exact SHA,
  including `ref: ${{ github.sha }}`. This lets unpublished commits and synthetic
  merge commits run without fetching them from GitHub. Setup repository checkout
  still executes normally at the workflow's pinned revision.
- `OPENBF2_ACT_PUBLIC_CHECKOUT=1` satisfies checkout's required token input with a
  synthetic placeholder. The wrapper explicitly supplies an empty act token and
  clears Git's GitHub Authorization header using command-level Git configuration.
  Public repositories are fetched anonymously; a private setup repository fails.

The wrapper builds the pinned source and runs the compatibility tests before using
it. Remove each patch when the pinned upstream version supplies equivalent
behavior and the artifact/snapshot preflight passes without it.
