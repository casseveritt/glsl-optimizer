#include "regal_glsl.h"
#include "ast.h"
#include "glsl_parser_extras.h"
#include "glsl_parser.h"
#include "ir_optimization.h"
#include "ir_print_glsl_visitor.h"
#include "ir_print_visitor.h"
#include "loop_analysis.h"
#include "program.h"
#include "linker.h"


extern "C" struct gl_shader *
_mesa_new_shader(struct gl_context *ctx, GLuint name, GLenum type);


static void
initialize_mesa_context(struct gl_context *ctx, gl_api api)
{
   memset(ctx, 0, sizeof(*ctx));

   ctx->API = api;

   ctx->Extensions.ARB_fragment_coord_conventions = GL_TRUE;
   ctx->Extensions.EXT_texture_array = GL_TRUE;
   ctx->Extensions.NV_texture_rectangle = GL_TRUE;
   ctx->Extensions.ARB_shader_texture_lod = GL_TRUE;

   // Enable opengl es extensions we care about here
   if (api == API_OPENGLES2)
   {
	   ctx->Extensions.OES_standard_derivatives = GL_TRUE;
	   ctx->Extensions.EXT_shadow_samplers = GL_TRUE;
   }

   ctx->Const.GLSLVersion = 140;

   /* 1.20 minimums. */
   ctx->Const.MaxLights = 8;
   ctx->Const.MaxClipPlanes = 8;
   ctx->Const.MaxTextureUnits = 2;

   /* allow high amount */
   ctx->Const.MaxTextureCoordUnits = 16;

   ctx->Const.VertexProgram.MaxAttribs = 16;
   ctx->Const.VertexProgram.MaxUniformComponents = 512;
   ctx->Const.MaxVarying = 8;
   ctx->Const.MaxVertexTextureImageUnits = 0;
   ctx->Const.MaxCombinedTextureImageUnits = 2;
   ctx->Const.MaxTextureImageUnits = 2;
   ctx->Const.FragmentProgram.MaxUniformComponents = 64;

   ctx->Const.MaxDrawBuffers = 2;

   ctx->Driver.NewShader = _mesa_new_shader;
}


struct regal_glsl_ctx {
	regal_glsl_ctx () {
		mem_ctx = ralloc_context (NULL);
		initialize_mesa_context (&mesa_ctx, API_OPENGL);
	}
	~regal_glsl_ctx() {
		ralloc_free (mem_ctx);
	}
	struct gl_context mesa_ctx;
	void* mem_ctx;
};

regal_glsl_ctx* regal_glsl_initialize ()
{
	return new regal_glsl_ctx();
}

void regal_glsl_cleanup (regal_glsl_ctx* ctx)
{
	delete ctx;
	_mesa_glsl_release_types();
	_mesa_glsl_release_functions();
}


struct regal_glsl_shader
{
	static void* operator new(size_t size, void *mem_ctx)
	{
		void *node;
		node = ralloc_size(mem_ctx, size);
		assert(node != NULL);
		return node;
	}
	static void operator delete(void *node)
	{
		ralloc_free(node);
	}

	regal_glsl_shader ()
		: state( NULL )
    , rawOutput(0)
		, optimizedOutput(0)
		, status(false)
	{
		infoLog = "Shader not compiled yet";
		
		whole_program = rzalloc (NULL, struct gl_shader_program);
		assert(whole_program != NULL);
		whole_program->InfoLog = ralloc_strdup(whole_program, "");
		
		whole_program->Shaders = reralloc(whole_program, whole_program->Shaders, struct gl_shader *, whole_program->NumShaders + 1);
		assert(whole_program->Shaders != NULL);
		
		shader = rzalloc(whole_program, gl_shader);
		whole_program->Shaders[whole_program->NumShaders] = shader;
		whole_program->NumShaders++;
	}
	
	~regal_glsl_shader()
	{
    ralloc_free (shader->ir);
    ralloc_free (state);
		for (unsigned i = 0; i < MESA_SHADER_TYPES; i++)
			ralloc_free(whole_program->_LinkedShaders[i]);
		ralloc_free(whole_program);
	}
	
	struct gl_shader_program* whole_program;
	struct gl_shader* shader;
  _mesa_glsl_parse_state* state;
  regal_glsl_ctx * ctx;

	char*	rawOutput;
	char*	optimizedOutput;
	const char*	infoLog;
	bool	status;
};

static inline void debug_print_ir (const char* name, exec_list* ir, _mesa_glsl_parse_state* state, void* memctx)
{
	#if 1
	printf("**** %s:\n", name);
	//_mesa_print_ir (ir, state);
	char* foobar = _mesa_print_ir_glsl(ir, state, ralloc_strdup(memctx, ""), kPrintGlslFragment);
	printf( "%s", foobar);
	validate_ir_tree(ir);
	#endif
}

regal_glsl_shader* regal_glsl_parse (regal_glsl_ctx* ctx, regal_glsl_shader_type type, const char* shaderSource)
{
	regal_glsl_shader* shader = new (ctx->mem_ctx) regal_glsl_shader ();
  shader->ctx = ctx;

	PrintGlslMode printMode;
	switch (type) {
    case kRegalGlslShaderVertex: shader->shader->Type = GL_VERTEX_SHADER; printMode = kPrintGlslVertex; break;
    case kRegalGlslShaderFragment: shader->shader->Type = GL_FRAGMENT_SHADER; printMode = kPrintGlslFragment; break;
    default:
      shader->infoLog = ralloc_asprintf (ctx->mem_ctx, "Unknown shader type %d", (int)type);
      shader->status = false;
      return shader;
	}

	_mesa_glsl_parse_state* state = new (ctx->mem_ctx) _mesa_glsl_parse_state (&ctx->mesa_ctx, shader->shader->Type, ctx->mem_ctx);
  shader->state = state;
	state->error = 0;

  state->error = glcpp_preprocess (state, &shaderSource, &state->info_log, state->extensions, ctx->mesa_ctx.API);
  if (state->error)	{
    shader->status = !state->error;
    shader->infoLog = state->info_log;
    return shader;
  }

	_mesa_glsl_lexer_ctor (state, shaderSource);
	_mesa_glsl_parse (state);
	_mesa_glsl_lexer_dtor (state);

	exec_list* ir = new (ctx->mem_ctx) exec_list();
	shader->shader->ir = ir;

	if (!state->error && !state->translation_unit.is_empty())
		_mesa_ast_to_hir (ir, state);

	// Un-optimized output
	if (!state->error) {
		validate_ir_tree(ir);
		shader->rawOutput = _mesa_print_ir_glsl(ir, state, ralloc_strdup(ctx->mem_ctx, ""), printMode);
	}

	return shader;
}

void regal_glsl_gen_output( regal_glsl_shader * shader ) {
  
  // shorthand
  _mesa_glsl_parse_state * state = shader->state;
  exec_list * ir = shader->shader->ir;
  regal_glsl_ctx *ctx = shader->ctx;
  
  PrintGlslMode printMode;
	switch (shader->shader->Type) {
    case GL_VERTEX_SHADER: printMode = kPrintGlslVertex; break;
    case GL_FRAGMENT_SHADER: printMode = kPrintGlslFragment; break;
    default:
      shader->infoLog = ralloc_asprintf (ctx->mem_ctx, "Unknown shader type %d", (int)shader->shader->Type);
      shader->status = false;
      return;
	}

  
	// Link built-in functions
	shader->shader->symbols = state->symbols;
	memcpy(shader->shader->builtins_to_link, state->builtins_to_link, sizeof(shader->shader->builtins_to_link[0]) * state->num_builtins_to_link);
	shader->shader->num_builtins_to_link = state->num_builtins_to_link;
	
	if (!state->error && !ir->is_empty())	{
		gl_shader * linked_shader =
    link_intrastage_shaders(ctx->mem_ctx, &ctx->mesa_ctx, shader->whole_program, shader->whole_program->Shaders, shader->whole_program->NumShaders);
		if (!linked_shader) {
			shader->status = false;
			shader->infoLog = shader->whole_program->InfoLog;
			return;
		}
		ir = linked_shader->ir;
		
		//debug_print_ir ("==== After link ====", ir, state, ctx->mem_ctx);
	}
	
	// Do optimization post-link
	if ( false && !state->error && !ir->is_empty())	{
		validate_ir_tree(ir);
	}
	
	// Final optimized output
	if (!state->error) {
		shader->optimizedOutput = _mesa_print_ir_glsl(ir, state, ralloc_strdup(ctx->mem_ctx, ""), printMode);
	}
  
	shader->status = !state->error;
	shader->infoLog = state->info_log;
}


class add_alpha_test : public ir_hierarchical_visitor {
public:
  
  add_alpha_test( RegalAlphaFunc raf ) : func( raf ), alphaRef( NULL ), fragColor( NULL ) {}
  
  virtual ir_visitor_status visit(ir_variable *var) {
    if( fragColor == NULL && var->type == glsl_type::vec4_type && ( var->mode == ir_var_out || var->mode == ir_var_inout ) ) {
      fragColor = var;
    }
    return visit_continue;
  }

  virtual ir_visitor_status visit_enter(ir_function *ir_f) {
    printf( "add_alpha_test: visit_enter function %s\n", ir_f->name );
    if( strcmp( ir_f->name, "main") != 0 ) {
      return visit_continue;
    }
    void * ctx = ralloc_parent( ir_f );
    alphaRef = new(ctx) ir_variable( glsl_type::float_type, "rglAlphaRef", ir_var_uniform, glsl_precision_undefined);
    ir_f->insert_before( alphaRef );
    return visit_continue;
  }

  
  virtual ir_visitor_status visit_leave(ir_function_signature *ir_fs) {
    printf( "add_alpha_test: visit_leave function_signature %s\n", ir_fs->function_name() );
    if( strcmp( ir_fs->function_name(), "main") != 0 ) {
      return visit_continue;
    }
    void * ctx = ralloc_parent( ir_fs );
    ir_rvalue * test = NULL;
    ir_rvalue * a = new(ctx) ir_swizzle( new(ctx) ir_dereference_variable( fragColor ), 3, 0, 0, 0, 1 );
    ir_rvalue * ref = new(ctx) ir_dereference_variable( alphaRef );
    switch( func ) {
      case RAF_Less:     test = new(ctx) ir_expression( ir_binop_less,    glsl_type::bool_type, a, ref ); break;
      case RAF_Lequal:   test = new(ctx) ir_expression( ir_binop_lequal,  glsl_type::bool_type, a, ref ); break;
      case RAF_Equal:    test = new(ctx) ir_expression( ir_binop_equal,   glsl_type::bool_type, a, ref ); break;
      case RAF_Gequal:   test = new(ctx) ir_expression( ir_binop_gequal,  glsl_type::bool_type, a, ref ); break;
      case RAF_Greater:  test = new(ctx) ir_expression( ir_binop_greater, glsl_type::bool_type, a, ref ); break;
      case RAF_NotEqual: test = new(ctx) ir_expression( ir_binop_nequal,  glsl_type::bool_type, a, ref ); break;
      case RAF_Never:    test = new(ctx) ir_constant( false ); break;
      case RAF_Always:   test = new(ctx) ir_constant( true );  break;
      default:
        // assert
        break;
    }
    ir_if * ifalphafailed = new(ctx) ir_if( new(ctx) ir_expression( ir_unop_logic_not, test ) );
    ifalphafailed->then_instructions.push_head( new(ctx) ir_discard() );
    ir_fs->body.get_tail()->insert_after( ifalphafailed );
    
    return visit_continue;
  }

  RegalAlphaFunc func;
  ir_variable *alphaRef;
  ir_variable *fragColor;
};

void regal_glsl_add_alpha_test( regal_glsl_shader * shader, RegalAlphaFunc func ) {

  add_alpha_test v(func);
  
  visit_list_elements(&v, shader->shader->ir );
  validate_ir_tree( shader->shader->ir );
}


void regal_glsl_shader_delete (regal_glsl_shader* shader)
{
	delete shader;
}

bool regal_glsl_get_status (regal_glsl_shader* shader)
{
	return shader->status;
}

const char* regal_glsl_get_output (regal_glsl_shader* shader)
{
	return shader->optimizedOutput;
}

const char* regal_glsl_get_raw_output (regal_glsl_shader* shader)
{
	return shader->rawOutput;
}

const char* regal_glsl_get_log (regal_glsl_shader* shader)
{
	return shader->infoLog;
}
