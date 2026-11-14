# [CHOSEN, not recovered] Generate upstream prerequisites without building the renderer.
.PHONY: shaderproto-headers
shaderproto-headers: $(widl_headers) include/private/spirv_grammar.h include/private/vkd3d_version.h
