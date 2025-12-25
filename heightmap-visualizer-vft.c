/* Always include this in all plug-ins */
#include <epoxy/gl.h>
#include <libgimp/gimpui.h>
#include <libgimp/gimp.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>

/* The name of my PDB procedure */
#define PLUG_IN_PROC "heightmap-visualizer-vft"
#define PLUG_IN_BINARY "heightmap-visualizer-vft"

/* Our custom class HeightmapVisualizer is derived from GimpPlugIn. */
struct _HeightmapVisualizer
{
  GimpPlugIn parent_instance;
};

#define HEIGHTMAP_VISUALIZER_TYPE (heightmap_visualizer_get_type())
G_DECLARE_FINAL_TYPE (HeightmapVisualizer, heightmap_visualizer, HEIGHTMAP_VISUALIZER,, GimpPlugIn)


/* Declarations */


FILE* logfile = NULL;

static const char gradient_name[] = "heightmap_visualizer_vft_default_gradient";


static void init_log() {
  logfile = fopen("logfile.txt", "w");
}

static void write_log(const char* stuff) {
  fprintf(logfile, "%s", stuff);
  fflush(logfile);
}


static GList* heightmap_visualizer_query_procedures
  (GimpPlugIn *plug_in);
  
static GimpProcedure* heightmap_visualizer_create_procedure
  (GimpPlugIn* plug_in, const gchar *name);
  
static GimpValueArray* heightmap_visualizer_run
  (GimpProcedure*procedure, GimpRunMode run_mode, GimpImage *image,
  GimpDrawable **drawables, GimpProcedureConfig *config, gpointer run_data);


G_DEFINE_TYPE (HeightmapVisualizer, heightmap_visualizer, GIMP_TYPE_PLUG_IN)

static void heightmap_visualizer_class_init (HeightmapVisualizerClass *klass) {
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = heightmap_visualizer_query_procedures;
  plug_in_class->create_procedure = heightmap_visualizer_create_procedure;
}

static void heightmap_visualizer_init (HeightmapVisualizer *heightmap_visualizer) {
}

static GList* heightmap_visualizer_query_procedures (GimpPlugIn *plug_in) {
  return g_list_append (NULL, g_strdup (PLUG_IN_PROC));
}

static GimpProcedure* heightmap_visualizer_create_procedure
  (GimpPlugIn  *plug_in, const gchar *name) {
  GimpProcedure *procedure = NULL;

  if (g_strcmp0 (name, PLUG_IN_PROC) == 0) {
    procedure = gimp_image_procedure_new
      (plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN, heightmap_visualizer_run, NULL, NULL);

    gimp_procedure_set_sensitivity_mask(procedure, GIMP_PROCEDURE_SENSITIVE_ALWAYS);

    gimp_procedure_set_menu_label(procedure, "Heightmap Visualizer");
    gimp_procedure_add_menu_path(procedure, "<Image>/View/");

    gimp_procedure_set_documentation
      (procedure, "Opens a 3d visualization of a heightmap of the layer",
      NULL, NULL);

    gimp_procedure_set_attribution
      (procedure, "vft32", "vft32", "Oct 2025");
      
    gimp_procedure_add_int_argument
      (procedure, "min-height", "Minimum Height",
      "the height corresponding to darkest colors",
      -1000, 1000, 0, G_PARAM_READWRITE);

    gimp_procedure_add_int_argument
      (procedure, "max-height", "Maximum Height",
      "the height corresponding to brightest colors",
      -1000, 1000, 100, G_PARAM_READWRITE);

      
    gimp_procedure_add_int_argument
      (procedure, "pixel-spacing", "Pixel Spacing",
      "the horizontal spacing between each pixel",
      0, 50, 8, G_PARAM_READWRITE);

    gimp_procedure_add_boolean_argument
      (procedure, "render-slope", "Render Slope",
      "render the heightmap's color with slope values instead of height",
      FALSE, G_PARAM_READWRITE);

    
    gint fake_argc = 0;
    gchar** fake_argv = NULL;
    
    gegl_init(&fake_argc, &fake_argv);
    
    GimpGradient* def_grad_arg = gimp_gradient_get_by_name(gradient_name);
    
    if (def_grad_arg == NULL) {
      def_grad_arg = gimp_gradient_new(gradient_name);
      GeglColor* col_black = gegl_color_new("black");
      GeglColor* col_white = gegl_color_new("white");
      GeglColor* col_red = gegl_color_new("red");
      GeglColor* col_blue = gegl_color_new("blue");
            
      gdouble start = 0.0f;
      gdouble mid = 0.4f;
      gdouble end = 1.0f;
      
      gimp_gradient_segment_set_left_color(def_grad_arg, 0, col_black);
      
      gimp_gradient_segment_set_right_color(def_grad_arg, 0, col_red);
      
      //gimp_gradient_segment_set_left_color(def_grad_arg, 1, col_red);
      //gimp_gradient_segment_set_right_color(def_grad_arg, 1, col_blue);
      //gimp_gradient_segment_set_middle_pos (def_grad_arg, 1, mid, &mid);
      
    }
    
    gimp_procedure_add_gradient_argument
      (procedure, "visual-gradient", "Visual Gradient",
      "the color gradient sampled by the heightmap according to height or slope",
      FALSE, def_grad_arg, FALSE, G_PARAM_READWRITE);
  }

  return procedure;
}




// start of my gtk code
// ============================================================================


struct Vertex {
  float x;
  float y;
  float z;
  float r;
  float g;
  float b;
};

char strbuf[100];

unsigned int WIDTH = 800;
unsigned int HEIGHT = 600;

// procedure dialog variables
static gint max_height = 100;
static gint min_height = 0;
static gint pixel_spacing = 8;
static GimpGradient* visual_gradient = NULL;
static GimpDrawable* heightmap_layer = NULL;
static gboolean render_slope = false;

static GDateTime* start = NULL;
  
static float asp_ratio = 1.0f;

static bool heightmap_generated = false;

uint32_t shaderProgram;
uint32_t VAO;

static uint32_t n_idx = 6;

static int hm_width = 29;
static int hm_depth = 29;

GtkWidget* gl_area = NULL;

const char *vertexShaderSource = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aCol;\n"
    "uniform mat4 tnsf;\n"
    "out vec3 pass_col;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = tnsf * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "   pass_col = aCol;\n"
    "}\0";
const char *fragmentShaderSource = 
    "#version 330 core\n"
    "in vec3 pass_col;\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(pass_col, 1.0f);\n"
    "}\n\0";
    
static float lerp(float amt, float low, float hi) {
  return low + ((hi - low) * amt);
}


static void setup_heightmap() {
  write_log("heightmap 1\n");
  
  if (heightmap_layer == NULL) {
    write_log("heightmap can't find drawable\n");
    return;
  }

  hm_width = gimp_drawable_get_width(heightmap_layer);
  hm_depth = gimp_drawable_get_height(heightmap_layer);
  
  sprintf(strbuf, "img dims are: %d x %d\n", hm_width, hm_depth);
  write_log(strbuf);
  
  GeglBuffer* gb = gimp_drawable_get_buffer(heightmap_layer);
  
  const Babl* fmt = babl_format("RGB float");
  
  const GeglRectangle* gr = gegl_buffer_get_extent(gb);
  
  
  sprintf(strbuf, "buffer dims are: %d x %d\n", gr->width, gr->height);
  write_log(strbuf);
  
  write_log("format name: ");
  write_log(babl_get_name(fmt));
  write_log("\n");
  
  int size = gr->width * gr->height * babl_format_get_bytes_per_pixel(fmt);
  
  gpointer dest_buf = malloc(size);
  
  gegl_buffer_get(gb, gr, 1.0f, fmt, dest_buf, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_WHITE);
  
  write_log("heightmap 2\n");
  
  int n_vtx = hm_width * hm_depth;
  
  GeglColor** cols = gimp_gradient_get_uniform_samples
    (visual_gradient, 1001, FALSE);
    
  
  struct Vertex* vtx = malloc(sizeof(struct Vertex) * n_vtx);
  float r, g, b;
  for (int z = 0; z < hm_depth; z++) {
    for (int x = 0; x < hm_width; x++) {
      int idx = (z * hm_width + x) * 3;
      r = ((float*) dest_buf)[idx];
      g = ((float*) dest_buf)[idx + 1];
      b = ((float*) dest_buf)[idx + 2];
      
      float fx = ((float)x) * ((float) pixel_spacing);
      float fz = ((float)z) * ((float) pixel_spacing);
      float brightness = (r + g + b) / 3.0f;
      if (x == 0 && z == 0) {
        sprintf(strbuf, "brightness: %f\n", brightness);
        write_log(strbuf);
      }
      
      int sample_idx = (int)(brightness * 1000.0f);
      
      // lerp
      float y = lerp(brightness, min_height, max_height);
      int i = z * hm_width + x;
      vtx[i].x = fx;
      vtx[i].y = y;
      vtx[i].z = fz;
      if (!render_slope) {
        gdouble r, g, b, a;
        gegl_color_get_rgba(cols[sample_idx], &r, &g, &b, &a);
        vtx[i].r = r;
        vtx[i].g = g;
        vtx[i].b = b;
      }
    }
  }

  // slope calculations if needed
  if (render_slope) {
    for (int z = 0; z < hm_depth; z++) {
      for (int x = 0; x < hm_width; x++) {
        int i = z * hm_width + x;

        vec3 v_c = {vtx[i].x, vtx[i].y, vtx[i].z};
        
        vec3 v_xn;
        vec3 v_xp;
        vec3 v_zn;
        vec3 v_zp;
        // checking (x-1, z)
        if (x == 0) {
          glm_vec3_copy(v_c, v_xn);
        } else {
          struct Vertex v = vtx[i-1];
          v_xn[0] = v.x;
          v_xn[1] = v.y;
          v_xn[2] = v.z;
        }
        // checking (x+1, z)
        if (x == (hm_width - 1)) {
          glm_vec3_copy(v_c, v_xp);
        } else {
          struct Vertex v = vtx[i+1];
          v_xp[0] = v.x;
          v_xp[1] = v.y;
          v_xp[2] = v.z;
        }
        // checking (x, z-1)
        if (z == 0) {
          glm_vec3_copy(v_c, v_zn);
        } else {
          struct Vertex v = vtx[i-hm_width];
          v_zn[0] = v.x;
          v_zn[1] = v.y;
          v_zn[2] = v.z;
        }
        // checking (x, z+1)
        if (z == (hm_depth - 1)) {
          glm_vec3_copy(v_c, v_zp);
        } else {
          struct Vertex v = vtx[i+hm_width];
          v_zp[0] = v.x;
          v_zp[1] = v.y;
          v_zp[2] = v.z;
        }

        vec3 v_dx;
        vec3 v_dz;

        glm_vec3_sub(v_xp, v_xn, v_dx);
        glm_vec3_sub(v_zp, v_zn, v_dz);

        vec3 v_n;
        vec3 v_r;
        vec3 v_u;

        glm_vec3_cross(v_dx, v_dz, v_n);
        glm_vec3_cross(GLM_YUP, v_n, v_r);
        glm_vec3_cross(v_n, v_r, v_u);

        if (v_u[1] < 0.0f) {
          glm_vec3_negate(v_u);
        }

        float slope;

        if (v_u[1] == 0.0f) {
          slope = 0.0f;
        } else {
          slope = v_u[1] / (sqrt((v_u[0] * v_u[0]) + (v_u[2] * v_u[2])));
        }





        // mapping a possibly infinite range into [0, 1]
        // by the function 1/(1+x)
        // slope infinity - 1000 -> 0 pix bucket
        // slope infinity - 501 -> 1 pix bucket
        // slope 500 - 330 -> 2 pix bucket
        // slope 330 - 249 -> 3 pix bucket
        // slope 250 - 200 -> 4
        // slope 200 - 167 -> 5
        // ...
        // 109-101 -> 9
        // ...
        // 18.2 - 17.9 -> 51
        // etc
        // higher precision as the slope decreases
        float slope_resolution = 240.0f;

        float encoded_slope = slope * slope_resolution;

        if (encoded_slope > 1000.0f) {
          encoded_slope = 1000.0f;
        }

        int sample_idx = (int)(encoded_slope);

        gdouble r, g, b, a;
        gegl_color_get_rgba(cols[sample_idx], &r, &g, &b, &a);
        vtx[i].r = r;
        vtx[i].g = g;
        vtx[i].b = b;
      }
    }
  }
  
  gimp_color_array_free(cols);
  
  n_idx = (hm_depth - 1) * ((hm_width * 2) + 2);
  uint32_t* idx = malloc(n_idx * sizeof(uint32_t));

  int i = 0;
  write_log("heightmap 3\n");
  
  sprintf(strbuf, "num idx: %d\n", n_idx);
  write_log(strbuf);

  for (int z = 0; z < hm_depth - 1; z++) {
    for (int x = 0; x < hm_width; x++) {
      idx[i] = x + z * hm_width; // (x, z)
      idx[i+1] = idx[i] + hm_width; // (x, z+1)
      i += 2;
    }
    // traverse to the starting point of the next line. Similar to "return carriage" in typewriters

    uint32_t next_start_idx = (z + 1) * hm_width; // (0, z+1)
    idx[i] = idx[i-1]; // (w, z+1)
    idx[i+1] = next_start_idx; // (0, z+1)
    i += 2;
  }
  
  if (n_idx != i) {
    sprintf(strbuf, "i (%d) != n_idx (%d)\n", i, n_idx);
    write_log(strbuf);
  }
  
  
  int vsize = sizeof(struct Vertex);
  
  uint32_t VBO, EBO;
  
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);
  
  write_log("heightmap 4\n");
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vsize * n_vtx, vtx, GL_STATIC_DRAW);
  
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vsize, (void*)(sizeof(float) * 0));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vsize, (void*)(sizeof(float) * 3));
  
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * n_idx, idx, GL_STATIC_DRAW);
  
  write_log("heightmap 5\n");
  
  if (idx != NULL) {
    free(idx);
    idx = NULL;
  }
  
  if (vtx != NULL) {
    free(vtx);
    vtx = NULL;
  }
  
  heightmap_generated = true;
}




/* We need to set up our state when we realize the GtkGLArea widget */
static void realize (GtkWidget *widget) {
  GdkGLContext* context;
  
  gtk_gl_area_make_current(GTK_GL_AREA(widget));
  
  if (gtk_gl_area_get_error(GTK_GL_AREA(widget)) != NULL) {
    write_log("error realize make gl area current\n");
    return;
  }
  
  
  unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);
  // check for shader compile errors
  int success;
  char infoLog[512];
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
      glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
      write_log("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n");
  }
  // fragment shader
  unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  // check for shader compile errors
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
      glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
      write_log("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n");
  }
  // link shaders
  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  // check for linking errors
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
      glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
      write_log("ERROR::SHADER::PROGRAM::LINKING_FAILED\n");
  }
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  write_log("realize\n");
  
  setup_heightmap();
  
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

/* We should tear down the state when unrealizing */
static void unrealize (GtkWidget *widget) {
  gtk_gl_area_make_current (GTK_GL_AREA (widget));

  if (gtk_gl_area_get_error (GTK_GL_AREA (widget)) != NULL)
    return;

  glDeleteProgram (shaderProgram);
}

static gboolean render (GtkGLArea    *area, GdkGLContext *context) {
  if (!heightmap_generated) {
    return FALSE;
  }
  
  
  if (start == NULL) {
    start = g_date_time_new_now_local();
  }
  
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  mat4 tnsf;
  mat4 view;
  mat4 proj;
  
  float fmax_height = (float) max_height;
  float fmin_height = (float) min_height;
  float fhm_width = (float) hm_width;
  float fhm_depth = (float) hm_depth;
  float fpixel_spacing = (float) pixel_spacing;
  
  
  GDateTime* now = g_date_time_new_now_local();
  
  float time_s = g_date_time_difference(now, start) / 1000000.0f;
  
  
  vec3 ctr = {fpixel_spacing * fhm_width / 2.0f, 0.0f, fpixel_spacing * fhm_depth / 2.0f};
  
  float cam_variation = fabs(fmax_height - fmin_height);
  
  float cam_height = ((sin(time_s / 8.2) / 2.0f) + 2.0f) * cam_variation * 3.0;
  
  float sqrt2 = sqrt(2.0f);
  
  vec3 eye = {fhm_width * fpixel_spacing / sqrt2, cam_height, fhm_depth * fpixel_spacing / sqrt2};
  
  glm_vec3_rotate(eye, time_s / 4.0f, GLM_YUP);
  
  glm_vec3_add(eye, ctr, eye);
  
  glm_lookat(eye, ctr, GLM_YUP, view);
  
  // approximate furthest distance we'll have to look
  float farplane = (fhm_depth + fhm_width) * fpixel_spacing * fpixel_spacing;
  
  float nearplane = farplane / 1000.0f;
  
  if (nearplane < 0.1f) {
    nearplane = 0.1f;
  }
  
  glm_perspective(glm_rad(45.0f), asp_ratio, farplane / 1000.0f, farplane, proj);
  
  glm_mat4_mul(proj, view, tnsf);
  
  uint32_t loc = glGetUniformLocation(shaderProgram, "tnsf");
  
  glUniformMatrix4fv(loc, 1, GL_FALSE, (GLfloat*) tnsf);

  // draw our first triangle
  glUseProgram(shaderProgram);
  glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
  //glDrawArrays(GL_TRIANGLES, 0, 6);
  glDrawElements(GL_TRIANGLE_STRIP, n_idx, GL_UNSIGNED_INT, 0);
  glFlush ();
  gtk_gl_area_queue_render (area);
  return TRUE;
}

static void on_axis_value_change (void) {
  gtk_widget_queue_draw (gl_area);
}

static void resize(GtkGLArea* area, gint width, gint height, gpointer data) {
  if (height != 0) {
    asp_ratio = ((float)width) / ((float)height);
  }
}




static GimpValueArray *
heightmap_visualizer_run (GimpProcedure        *procedure,
                 GimpRunMode           run_mode,
                 GimpImage            *image,
                 GimpDrawable        **drawables,
                 GimpProcedureConfig  *config,
                 gpointer              run_data)
{
  GimpTextLayer *text_layer;
  GimpLayer     *parent   = NULL;
  gint           position = 0;
  gint           n_drawables;

  gchar         *text;
  GimpFont      *font;
  gint           size;
  GimpUnit      *unit;
  
  // clear file for new viewing
  init_log();
  freopen("output.txt", "w", stdout);
  freopen("error.txt", "w", stderr);
  write_log("main\n");

  n_drawables = gimp_core_object_array_get_length ((GObject **) drawables);

  if (n_drawables > 1) {
    GError *error = NULL;

    g_set_error
      (&error, GIMP_PLUG_IN_ERROR, 0,
      "Procedure '%s' works with zero or one layer.", PLUG_IN_PROC);

    fclose(logfile);
    return gimp_procedure_new_return_values
      (procedure, GIMP_PDB_CALLING_ERROR, error);
      
  } else if (n_drawables == 1) {
    GimpDrawable *drawable = drawables[0];
    
    heightmap_layer = drawable;

    if (!GIMP_IS_LAYER(drawable)) {
      GError *error = NULL;

      g_set_error
        (&error, GIMP_PLUG_IN_ERROR, 0,
        "Procedure '%s' works with layers only.", PLUG_IN_PROC);

      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_CALLING_ERROR,
                                               error);
    }

    parent   = GIMP_LAYER (gimp_item_get_parent (GIMP_ITEM (drawable)));
    position = gimp_image_get_item_position (image, GIMP_ITEM (drawable));
  }

  if (run_mode == GIMP_RUN_INTERACTIVE) {
    GtkWidget *dialog;

    gimp_ui_init(PLUG_IN_BINARY);
    dialog = gimp_procedure_dialog_new
      (procedure, GIMP_PROCEDURE_CONFIG(config), "Heightmap Visualizer Settings");
      
    gimp_procedure_dialog_fill(GIMP_PROCEDURE_DIALOG(dialog), NULL);

    if (!gimp_procedure_dialog_run (GIMP_PROCEDURE_DIALOG(dialog))) {
      fclose(logfile);
      return gimp_procedure_new_return_values(procedure, GIMP_PDB_CANCEL, NULL);
    }
  }

  write_log("main 1\n");
  g_object_get
    (config,
    "max-height",       &max_height,
    "min-height",       &min_height,
    "pixel-spacing",    &pixel_spacing,
    "visual-gradient",  &visual_gradient,
    "render-slope",     &render_slope,
    NULL);
    
  write_log("main 2\n");
  
  int argc = 0;
  char** argv = NULL;
  
  GtkWidget *window, *box;
  /* initialize gtk */
  gtk_init(&argc, &argv);
  /* Create new top level window. */
  window = gtk_window_new( GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW(window),WIDTH,HEIGHT);
  gtk_window_set_title(GTK_WINDOW(window), "Heightmap");
  gtk_container_set_border_width(GTK_CONTAINER(window), 0);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  g_object_set (box, "margin", 0, NULL);
  gtk_box_set_spacing (GTK_BOX (box), 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  gl_area = gtk_gl_area_new ();
  gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl_area), TRUE);
  gtk_box_pack_start (GTK_BOX(box), gl_area,1,1, 0);
  write_log("main 3\n");
  /* We need to initialize and free GL resources, so we use
  * the realize and unrealize signals on the widget
  */
  g_signal_connect (gl_area, "realize", G_CALLBACK (realize), NULL);
  g_signal_connect (gl_area, "unrealize", G_CALLBACK (unrealize), NULL);

  /* The main "draw" call for GtkGLArea */
  g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);
  g_signal_connect (gl_area, "resize", G_CALLBACK (resize), NULL);
  /* Quit form main if got delete event */
  g_signal_connect(G_OBJECT(window), "delete-event",
                 G_CALLBACK(gtk_main_quit), NULL);
  gtk_widget_show_all(GTK_WIDGET(window));
  write_log("main 4\n");
  gtk_main();
  write_log("main 5\n");
  
  fclose(logfile);


  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}

/* Generate needed main() code */
GIMP_MAIN (HEIGHTMAP_VISUALIZER_TYPE)


