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
	kRegalGlslShaderVertex = 0,
	kRegalGlslShaderFragment,
};

enum RegalAlphaFunc {
  RAF_Less = 0,
  RAF_Lequal,
  RAF_Equal,
  RAF_Gequal,
  RAF_Greater,
  RAF_NotEqual,
  RAF_Never,
  RAF_Always
};

regal_glsl_ctx* regal_glsl_initialize ();
void regal_glsl_cleanup (regal_glsl_ctx* ctx);

regal_glsl_shader* regal_glsl_parse (regal_glsl_ctx* ctx, regal_glsl_shader_type type, const char* shaderSource);
void regal_glsl_replace_builtins( regal_glsl_shader * shader );
void regal_glsl_add_alpha_test( regal_glsl_shader * shader, RegalAlphaFunc func );
void regal_glsl_gen_output( regal_glsl_shader * shader );

bool regal_glsl_get_status (regal_glsl_shader* shader);
const char* regal_glsl_get_output (regal_glsl_shader* shader);
const char* regal_glsl_get_raw_output (regal_glsl_shader* shader);
const char* regal_glsl_get_log (regal_glsl_shader* shader);
void regal_glsl_shader_delete (regal_glsl_shader* shader);


#endif /* GLSL_OPTIMIZER_H */
