#pragma once
#ifndef GLSL_OPTIMIZER_H
#define GLSL_OPTIMIZER_H

/*
 Main GLSL optimizer interface.
 See ../../README.md for more instructions.

 General usage:

 ctx = regal_glsl_initialize();
 for (lots of shaders) {
   shader = regal_glsl_optimize (ctx, shaderType, shaderSource, options);
   if (regal_glsl_get_status (shader)) {
     newSource = regal_glsl_get_output (shader);
   } else {
     errorLog = regal_glsl_get_log (shader);
   }
   regal_glsl_shader_delete (shader);
 }
 regal_glsl_cleanup (ctx);
*/

struct regal_glsl_shader;
struct regal_glsl_ctx;

enum regal_glsl_shader_type {
	kGlslOptShaderVertex = 0,
	kGlslOptShaderFragment,
};

// Options flags for glsl_optimize
enum regal_glsl_options {
	kGlslOptionSkipPreprocessor = (1<<0), // Skip preprocessing shader source. Saves some time if you know you don't need it.
	kGlslOptionNotFullShader = (1<<1), // Passed shader is not the full shader source. This makes some optimizations weaker.
};

regal_glsl_ctx* regal_glsl_initialize (bool openglES);
void regal_glsl_cleanup (regal_glsl_ctx* ctx);

regal_glsl_shader* regal_glsl_optimize (regal_glsl_ctx* ctx, regal_glsl_shader_type type, const char* shaderSource, unsigned options);
bool regal_glsl_get_status (regal_glsl_shader* shader);
const char* regal_glsl_get_output (regal_glsl_shader* shader);
const char* regal_glsl_get_raw_output (regal_glsl_shader* shader);
const char* regal_glsl_get_log (regal_glsl_shader* shader);
void regal_glsl_shader_delete (regal_glsl_shader* shader);


#endif /* GLSL_OPTIMIZER_H */