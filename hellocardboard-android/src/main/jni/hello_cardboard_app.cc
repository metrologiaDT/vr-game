/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hello_cardboard_app.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

#include <array>
#include <cmath>
#include <fstream>

#include "cardboard.h"
#include <chrono>  // <--- AGREGA ESTA LÍNEA AQUÍ

namespace ndk_hello_cardboard {

namespace {

// The objects are about 1 meter in radius, so the min/max target distance are
// set so that the objects are always within the room (which is about 5 meters
// across) and the reticle is always closer than any objects.
constexpr float kMinTargetDistance = 2.5f;
constexpr float kMaxTargetDistance = 3.5f;
constexpr float kMinTargetHeight = 0.5f;
constexpr float kMaxTargetHeight = kMinTargetHeight + 3.0f;

constexpr float kDefaultFloorHeight = -0.1f;

// 6 Hz cutoff frequency for the velocity filter of the head tracker.
constexpr int kVelocityFilterCutoffFrequency = 6;

constexpr uint64_t kPredictionTimeWithoutVsyncNanos = 50000000;

// Angle threshold for determining whether the controller is pointing at the
// object.
constexpr float kAngleLimit = 0.2f;

// Number of different possible targets
constexpr int kTargetMeshCount = 3;

// --- VARIABLES DE ANIMACIÓN Y POSICIÓN JUEGO COGNITIVO ---
    std::array<float, 3> g_current_target_pos = {0.0f, 0.0f, -3.0f};
    bool g_is_animating = false;
    float g_current_scale = 1.0f;
    float g_current_angle = 0.0f;

// NUEVAS VARIABLES DE PROGRESO Y ESTADOS
    int g_score = 0;
    int g_current_level = 1;
    const int MAX_PELOTAS = 10;

// Estados posibles del juego
    enum GameState { PANTALLA_INICIO, JUGANDO, VICTORIA, ESPERANDO_REINICIO };
    GameState g_game_state = PANTALLA_INICIO;

// Temporizador para el reinicio
    auto g_win_time = std::chrono::steady_clock::now();
// ---------------------------------------------------------

// Simple shaders to render .obj files without any lighting.
constexpr const char* kObjVertexShader =
    R"glsl(
    uniform mat4 u_MVP;
    attribute vec4 a_Position;
    attribute vec2 a_UV;
    varying vec2 v_UV;

    void main() {
      v_UV = a_UV;
      gl_Position = u_MVP * a_Position;
    })glsl";

constexpr const char* kObjFragmentShader =
    R"glsl(
    precision mediump float;

    uniform sampler2D u_Texture;
    varying vec2 v_UV;

    void main() {
      // The y coordinate of this sample's textures is reversed compared to
      // what OpenGL expects, so we invert the y coordinate.
      gl_FragColor = texture2D(u_Texture, vec2(v_UV.x, 1.0 - v_UV.y));
    })glsl";

}  // anonymous namespace

HelloCardboardApp::HelloCardboardApp(JavaVM* vm, jobject obj,
                                     jobject asset_mgr_obj)
    : head_tracker_(nullptr),
      lens_distortion_(nullptr),
      distortion_renderer_(nullptr),
      screen_params_changed_(false),
      device_params_changed_(false),
      screen_width_(0),
      screen_height_(0),
      depthRenderBuffer_(0),
      framebuffer_(0),
      texture_(0),
      obj_program_(0),
      obj_position_param_(0),
      obj_uv_param_(0),
      obj_modelview_projection_param_(0),
      target_object_meshes_(kTargetMeshCount),
      target_object_not_selected_textures_(kTargetMeshCount),
      target_object_selected_textures_(kTargetMeshCount),
      cur_target_object_(RandomUniformInt(kTargetMeshCount)) {
  JNIEnv* env;
  vm->GetEnv((void**)&env, JNI_VERSION_1_6);
  java_asset_mgr_ = env->NewGlobalRef(asset_mgr_obj);
  asset_mgr_ = AAssetManager_fromJava(env, asset_mgr_obj);

  Cardboard_initializeAndroid(vm, obj);
  head_tracker_ = CardboardHeadTracker_create();
  CardboardHeadTracker_setLowPassFilter(head_tracker_,
                                        kVelocityFilterCutoffFrequency);
}

HelloCardboardApp::~HelloCardboardApp() {
  CardboardHeadTracker_destroy(head_tracker_);
  CardboardLensDistortion_destroy(lens_distortion_);
  CardboardDistortionRenderer_destroy(distortion_renderer_);
}

void HelloCardboardApp::OnSurfaceCreated(JNIEnv* env) {
  const int obj_vertex_shader =
      LoadGLShader(GL_VERTEX_SHADER, kObjVertexShader);
  const int obj_fragment_shader =
      LoadGLShader(GL_FRAGMENT_SHADER, kObjFragmentShader);

  obj_program_ = glCreateProgram();
  glAttachShader(obj_program_, obj_vertex_shader);
  glAttachShader(obj_program_, obj_fragment_shader);
  glLinkProgram(obj_program_);
  glUseProgram(obj_program_);

  CHECKGLERROR("Obj program");

  obj_position_param_ = glGetAttribLocation(obj_program_, "a_Position");
  obj_uv_param_ = glGetAttribLocation(obj_program_, "a_UV");
  obj_modelview_projection_param_ = glGetUniformLocation(obj_program_, "u_MVP");

  CHECKGLERROR("Obj program params");

  HELLOCARDBOARD_CHECK(room_.Initialize(obj_position_param_, obj_uv_param_,
                                        "salaOBJ.obj", asset_mgr_));


    HELLOCARDBOARD_CHECK(
        room_tex_.Initialize(env, java_asset_mgr_, "textura_sala.png"));


// Cargar parte 1 - (room con textura1)
//    HELLOCARDBOARD_CHECK(room_.Initialize(obj_position_param_, obj_uv_param_,
//                                          "Bar1.obj", asset_mgr_));

//    HELLOCARDBOARD_CHECK(
//            room_tex_.Initialize(env, java_asset_mgr_, "Bar_A_BaseColor.png"));

    // Cargar parte 2
//    HELLOCARDBOARD_CHECK(room_parte2_.Initialize(obj_position_param_, obj_uv_param_,
//                                                 "Bar2.obj", asset_mgr_));

//    HELLOCARDBOARD_CHECK(
//            room_tex_parte2_.Initialize(env, java_asset_mgr_, "Bar_B_BaseColor.png"));

// Cargar el plano generado por código
  HELLOCARDBOARD_CHECK(ui_mesh_.Initialize(
          obj_position_param_, obj_uv_param_, "plano_ui.obj", asset_mgr_));

  // Cargar las texturas 2D
  HELLOCARDBOARD_CHECK(tex_inicio_.Initialize(env, java_asset_mgr_, "inicio.png"));
  HELLOCARDBOARD_CHECK(tex_victoria_.Initialize(env, java_asset_mgr_, "victoria.png"));
  HELLOCARDBOARD_CHECK(tex_reinicio_.Initialize(env, java_asset_mgr_, "reinicio.png"));
  HELLOCARDBOARD_CHECK(tex_puntero_.Initialize(env, java_asset_mgr_, "puntero.png"));


  HELLOCARDBOARD_CHECK(target_object_meshes_[0].Initialize(
      obj_position_param_, obj_uv_param_, "Icosahedron.obj", asset_mgr_));
  HELLOCARDBOARD_CHECK(target_object_not_selected_textures_[0].Initialize(
      env, java_asset_mgr_, "Icosahedron_Blue_BakedDiffuse.png"));
  HELLOCARDBOARD_CHECK(target_object_selected_textures_[0].Initialize(
      env, java_asset_mgr_, "Icosahedron_Pink_BakedDiffuse.png"));
  HELLOCARDBOARD_CHECK(target_object_meshes_[1].Initialize(
      obj_position_param_, obj_uv_param_, "QuadSphere.obj", asset_mgr_));
  HELLOCARDBOARD_CHECK(target_object_not_selected_textures_[1].Initialize(
      env, java_asset_mgr_, "QuadSphere_Blue_BakedDiffuse.png"));
  HELLOCARDBOARD_CHECK(target_object_selected_textures_[1].Initialize(
      env, java_asset_mgr_, "QuadSphere_Pink_BakedDiffuse.png"));
  HELLOCARDBOARD_CHECK(target_object_meshes_[2].Initialize(
      obj_position_param_, obj_uv_param_, "TriSphere.obj", asset_mgr_));
  HELLOCARDBOARD_CHECK(target_object_not_selected_textures_[2].Initialize(
      env, java_asset_mgr_, "TriSphere_Blue_BakedDiffuse.png"));
  HELLOCARDBOARD_CHECK(target_object_selected_textures_[2].Initialize(
      env, java_asset_mgr_, "TriSphere_Pink_BakedDiffuse.png"));

  // Target object first appears directly in front of user.
  model_target_ = GetTranslationMatrix({0.0f, 1.5f, kMinTargetDistance});

  CHECKGLERROR("OnSurfaceCreated");
}

void HelloCardboardApp::SetScreenParams(int width, int height) {
  screen_width_ = width;
  screen_height_ = height;
  screen_params_changed_ = true;
}

void HelloCardboardApp::OnDrawFrame() {
  if (!UpdateDeviceParams()) {
    return;
  }

  // Update Head Pose.
  head_view_ = GetPose();

  // Incorporate the floor height into the head_view
  head_view_ =
      head_view_ * GetTranslationMatrix({0.0f, kDefaultFloorHeight, 0.0f});


  // ==========================================================
  // INICIO DE LÓGICA DE JUEGO: ESTADOS, DWELL CLICK Y ANIMACIÓN
  // ==========================================================
  static auto start_look_time = std::chrono::steady_clock::now();
  static auto anim_start_time = std::chrono::steady_clock::now();
  static bool was_pointing = false;
  const int DWELL_TIME_MS = 2000;
  const float ANIM_DURATION_MS = 1000.0f;

  // Lógica para cambiar de VICTORIA a ESPERANDO_REINICIO tras 3 segundos
  if (g_game_state == VICTORIA) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed_since_win = std::chrono::duration_cast<std::chrono::seconds>(now - g_win_time).count();
    if (elapsed_since_win >= 3) {
      g_game_state = ESPERANDO_REINICIO;
    }
  }

  // Animación del objeto
  if (g_is_animating && g_game_state == JUGANDO) {
    auto now = std::chrono::steady_clock::now();
    float elapsed_anim = std::chrono::duration_cast<std::chrono::milliseconds>(now - anim_start_time).count();

    if (elapsed_anim >= ANIM_DURATION_MS) {
      g_is_animating = false;
      HideTarget();
    } else {
      float progress = elapsed_anim / ANIM_DURATION_MS;
      g_current_scale = 1.0f - progress;
      g_current_angle = progress * 3.14159f * 4.0f;
    }
  } else {
    // Si lo está mirando
    if (IsPointingAtTarget()) {
      if (!was_pointing) {
        start_look_time = std::chrono::steady_clock::now();
        was_pointing = true;
      } else {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_look_time).count();
        if (elapsed >= DWELL_TIME_MS) {

          if (g_game_state == JUGANDO) {
            g_is_animating = true;
            anim_start_time = std::chrono::steady_clock::now();
          } else {
            // Si no está jugando (ej. inicio o reinicio), activa la acción directo sin animación
            HideTarget();
          }
          was_pointing = false;
        }
      }
    } else {
      was_pointing = false;
    }
  }

  // Actualizamos la posición y rotación del objeto ACTIVO
  std::array<float, 4> rot_quat = {0.0f, std::sin(g_current_angle / 2.0f), 0.0f, std::cos(g_current_angle / 2.0f)};

  // Dependiendo del estado, la "mirilla" apunta a diferentes lugares
  if (g_game_state == PANTALLA_INICIO) {
    // El cartel de inicio flotará frente al jugador
    model_target_ = GetTranslationMatrix({0.0f, 1.5f, -2.5f});
  } else if (g_game_state == VICTORIA) {
    // El cartel de victoria flota en el centro
    model_target_ = GetTranslationMatrix({0.0f, 1.5f, -2.5f});
  } else if (g_game_state == ESPERANDO_REINICIO) {
    // Dibujamos el cartel de victoria arriba
    std::array<float, 16> cartel_array = modelview_projection_cartel_.ToGlArray();
    glUniformMatrix4fv(obj_modelview_projection_param_, 1, GL_FALSE, cartel_array.data());
    tex_victoria_.Bind();
    ui_mesh_.Draw();

    // Dibujamos el botón de reinicio más abajo
    std::array<float, 16> reinicio_array = modelview_projection_reinicio_.ToGlArray();
    glUniformMatrix4fv(obj_modelview_projection_param_, 1, GL_FALSE, reinicio_array.data());
    tex_reinicio_.Bind();
    ui_mesh_.Draw();
  } else {
    // Está JUGANDO, la matriz sigue a la pelota
    model_target_ = GetTranslationMatrix(g_current_target_pos) * Quatf::FromXYZW(&rot_quat[0]).ToMatrix();
  }
  // ==========================================================

  // Bind buffer
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

  // Bind buffer
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Draw eyes views
  for (int eye = 0; eye < 2; ++eye) {
    glViewport(eye == kLeft ? 0 : screen_width_ / 2, 0, screen_width_ / 2,
               screen_height_);

    Matrix4x4 eye_matrix = GetMatrixFromGlArray(eye_matrices_[eye]);
    Matrix4x4 eye_view = eye_matrix * head_view_;

    Matrix4x4 projection_matrix =
        GetMatrixFromGlArray(projection_matrices_[eye]);
    Matrix4x4 modelview_target = eye_view * model_target_;
    modelview_projection_target_ = projection_matrix * modelview_target;


    // Debajo de modelview_projection_target_ = projection_matrix * modelview_target;

    // 1. MATRIZ DEL PUNTERO (Fijo a 1.5 mts de la cara)
    Matrix4x4 model_puntero = GetTranslationMatrix({0.0f, 0.0f, -1.5f});
    // Achicamos el tamaño del plano matemático para que sea un cursor pequeño (escala al 5%)
    model_puntero.m[0][0] = 0.05f;
    model_puntero.m[1][1] = 0.05f;
    model_puntero.m[2][2] = 0.05f;
      modelview_projection_puntero_ = projection_matrix * (eye_matrix * model_puntero);


    // 2. MATRIZ DEL CARTEL (Flotando en el mundo estático)
    Matrix4x4 model_cartel = GetTranslationMatrix({0.0f, 1.5f, -2.5f});
    modelview_projection_cartel_ = projection_matrix * (eye_view * model_cartel);

    // 3. MATRIZ DEL BOTÓN DE REINICIO (NUEVO)
    Matrix4x4 model_reinicio = GetTranslationMatrix({0.0f, 0.5f, -2.5f});
    modelview_projection_reinicio_ = projection_matrix * (eye_view * model_reinicio);
    modelview_projection_room_ = projection_matrix * eye_view;


    // Draw room and target
    DrawWorld();
  }

  // Render
  CardboardDistortionRenderer_renderEyeToDisplay(
      distortion_renderer_, /* target_display = */ 0, /* x = */ 0, /* y = */ 0,
      screen_width_, screen_height_, &left_eye_texture_description_,
      &right_eye_texture_description_);

  CHECKGLERROR("onDrawFrame");
}

void HelloCardboardApp::OnTriggerEvent() {
  if (IsPointingAtTarget()) {
    HideTarget();
  }
}

void HelloCardboardApp::OnPause() { CardboardHeadTracker_pause(head_tracker_); }

void HelloCardboardApp::OnResume() {
  CardboardHeadTracker_resume(head_tracker_);

  // Parameters may have changed.
  device_params_changed_ = true;

  // Check for device parameters existence in external storage. If they're
  // missing, we must scan a Cardboard QR code and save the obtained parameters.
  uint8_t* buffer;
  int size;
  CardboardQrCode_getSavedDeviceParams(&buffer, &size);
  if (size == 0) {
    SwitchViewer();
  }
  CardboardQrCode_destroy(buffer);
}

void HelloCardboardApp::SwitchViewer() {
  CardboardQrCode_scanQrCodeAndSaveDeviceParams();
}

bool HelloCardboardApp::UpdateDeviceParams() {
  // Checks if screen or device parameters changed
  if (!screen_params_changed_ && !device_params_changed_) {
    return true;
  }

  // Get saved device parameters
  uint8_t* buffer;
  int size;
  CardboardQrCode_getSavedDeviceParams(&buffer, &size);

  // If there are no parameters saved yet, returns false.
  if (size == 0) {
    return false;
  }

  CardboardLensDistortion_destroy(lens_distortion_);
  lens_distortion_ = CardboardLensDistortion_create(buffer, size, screen_width_,
                                                    screen_height_);

  CardboardQrCode_destroy(buffer);

  GlSetup();

  CardboardDistortionRenderer_destroy(distortion_renderer_);
  const CardboardOpenGlEsDistortionRendererConfig config{kGlTexture2D};
  distortion_renderer_ = CardboardOpenGlEs2DistortionRenderer_create(&config);

  CardboardMesh left_mesh;
  CardboardMesh right_mesh;
  CardboardLensDistortion_getDistortionMesh(lens_distortion_, kLeft,
                                            &left_mesh);
  CardboardLensDistortion_getDistortionMesh(lens_distortion_, kRight,
                                            &right_mesh);

  CardboardDistortionRenderer_setMesh(distortion_renderer_, &left_mesh, kLeft);
  CardboardDistortionRenderer_setMesh(distortion_renderer_, &right_mesh,
                                      kRight);

  // Get eye matrices
  CardboardLensDistortion_getEyeFromHeadMatrix(lens_distortion_, kLeft,
                                               eye_matrices_[0]);
  CardboardLensDistortion_getEyeFromHeadMatrix(lens_distortion_, kRight,
                                               eye_matrices_[1]);
  CardboardLensDistortion_getProjectionMatrix(lens_distortion_, kLeft, kZNear,
                                              kZFar, projection_matrices_[0]);
  CardboardLensDistortion_getProjectionMatrix(lens_distortion_, kRight, kZNear,
                                              kZFar, projection_matrices_[1]);

  screen_params_changed_ = false;
  device_params_changed_ = false;

  CHECKGLERROR("UpdateDeviceParams");

  return true;
}

void HelloCardboardApp::GlSetup() {
  LOGD("GL SETUP");

  if (framebuffer_ != 0) {
    GlTeardown();
  }

  // Create render texture.
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screen_width_, screen_height_, 0,
               GL_RGB, GL_UNSIGNED_BYTE, 0);

  left_eye_texture_description_.texture = texture_;
  left_eye_texture_description_.left_u = 0;
  left_eye_texture_description_.right_u = 0.5;
  left_eye_texture_description_.top_v = 1;
  left_eye_texture_description_.bottom_v = 0;

  right_eye_texture_description_.texture = texture_;
  right_eye_texture_description_.left_u = 0.5;
  right_eye_texture_description_.right_u = 1;
  right_eye_texture_description_.top_v = 1;
  right_eye_texture_description_.bottom_v = 0;

  // Generate depth buffer to perform depth test.
  glGenRenderbuffers(1, &depthRenderBuffer_);
  glBindRenderbuffer(GL_RENDERBUFFER, depthRenderBuffer_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, screen_width_,
                        screen_height_);
  CHECKGLERROR("Create Render buffer");

  // Create render target.
  glGenFramebuffers(1, &framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture_, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depthRenderBuffer_);

  CHECKGLERROR("GlSetup");
}

void HelloCardboardApp::GlTeardown() {
  if (framebuffer_ == 0) {
    return;
  }
  glDeleteRenderbuffers(1, &depthRenderBuffer_);
  depthRenderBuffer_ = 0;
  glDeleteFramebuffers(1, &framebuffer_);
  framebuffer_ = 0;
  glDeleteTextures(1, &texture_);
  texture_ = 0;

  CHECKGLERROR("GlTeardown");
}

Matrix4x4 HelloCardboardApp::GetPose() {
  std::array<float, 4> out_orientation;
  std::array<float, 3> out_position;
  CardboardHeadTracker_getPose(
      head_tracker_, GetBootTimeNano() + kPredictionTimeWithoutVsyncNanos,
      kLandscapeLeft, &out_position[0], &out_orientation[0]);
  return GetTranslationMatrix(out_position) *
         Quatf::FromXYZW(&out_orientation[0]).ToMatrix();
}

void HelloCardboardApp::DrawWorld() {
  DrawRoom();

  glUseProgram(obj_program_);

  // Lógica para decidir qué dibujar según el estado (ver variables de la respuesta anterior)
  if (g_game_state == PANTALLA_INICIO) {
    std::array<float, 16> cartel_array = modelview_projection_cartel_.ToGlArray();
    glUniformMatrix4fv(obj_modelview_projection_param_, 1, GL_FALSE, cartel_array.data());
    tex_inicio_.Bind();
    ui_mesh_.Draw();
  }
  else if (g_game_state == VICTORIA) {
    std::array<float, 16> cartel_array = modelview_projection_cartel_.ToGlArray();
    glUniformMatrix4fv(obj_modelview_projection_param_, 1, GL_FALSE, cartel_array.data());
    tex_victoria_.Bind();
    ui_mesh_.Draw();
  }
  else if (g_game_state == ESPERANDO_REINICIO) {
    // Dibujamos victoria arriba...
    std::array<float, 16> cartel_array = modelview_projection_cartel_.ToGlArray();
    glUniformMatrix4fv(obj_modelview_projection_param_, 1, GL_FALSE, cartel_array.data());
    tex_victoria_.Bind();
    ui_mesh_.Draw();

    // ...y el botón de reinicio más abajo
    Matrix4x4 btn_mat = GetTranslationMatrix({0.0f, 0.5f, -2.5f});
    // (Nota: necesitas calcular el modelview_projection_btn en OnDrawFrame igual que el cartel)
    // tex_reinicio_.Bind();
    // ui_mesh_.Draw();
  }
  else if (g_game_state == JUGANDO) {
    // Llamamos a tu código original de dibujo de la pelota animada
    DrawTarget();
  }

  // --- DIBUJAR EL PUNTERO SIEMPRE AL FINAL (Para que quede superpuesto) ---
  // IMPORTANTE: Desactivar la prueba de profundidad momentáneamente para que el puntero atraviese paredes
  glDisable(GL_DEPTH_TEST);
  std::array<float, 16> puntero_array = modelview_projection_puntero_.ToGlArray();
  glUniformMatrix4fv(obj_modelview_projection_param_, 1, GL_FALSE, puntero_array.data());
  tex_puntero_.Bind();
  ui_mesh_.Draw();
  glEnable(GL_DEPTH_TEST); // Volver a activar para el siguiente frame
}

void HelloCardboardApp::DrawTarget() {
  glUseProgram(obj_program_);

  std::array<float, 16> target_array = modelview_projection_target_.ToGlArray();

  // --- APLICAR LA ESCALA DE LA ANIMACIÓN ---
  // Al multiplicar las primeras 3 columnas de la matriz, achicamos el objeto
  for (int i = 0; i < 12; ++i) {
    target_array[i] *= g_current_scale;
  }
  // -----------------------------------------

  glUniformMatrix4fv(obj_modelview_projection_param_, 1, GL_FALSE,
                     target_array.data());

  if (IsPointingAtTarget()) {
    target_object_selected_textures_[cur_target_object_].Bind();
  } else {
    target_object_not_selected_textures_[cur_target_object_].Bind();
  }
  target_object_meshes_[cur_target_object_].Draw();

  CHECKGLERROR("DrawTarget");
}

void HelloCardboardApp::DrawRoom() {
  glUseProgram(obj_program_);

  std::array<float, 16> room_array = modelview_projection_room_.ToGlArray();
  glUniformMatrix4fv(obj_modelview_projection_param_, 1, GL_FALSE,
                     room_array.data());

 // Dibujar la primera parte
  room_tex_.Bind();
  room_.Draw();


  // Dibujar la segunda parte (comparte la misma matriz de posición,
  // así que encajará perfectamente en su lugar)
 // room_tex_parte2_.Bind();
 // room_parte2_.Draw();


  CHECKGLERROR("DrawRoom");
}

    void HelloCardboardApp::HideTarget() {
        if (g_game_state == PANTALLA_INICIO) {
            // Si estamos en el inicio y miramos el cartel, empieza el juego
            g_game_state = JUGANDO;
            g_score = 0;
            g_current_target_pos = {0.0f, 0.0f, -3.0f}; // Posición de la 1ra pelota
        } else if (g_game_state == JUGANDO) {
            // Sumamos un punto
            g_score++;

            // ¿Ganó el juego?
            if (g_score >= MAX_PELOTAS) {
                g_game_state = VICTORIA;
                g_win_time = std::chrono::steady_clock::now(); // Guardamos la hora en que ganó
                return;
            }

            // LISTA DE LAS 10 POSICIONES EXACTAS
            // Ejes: X (Izquierda/Derecha), Y (Abajo/Arriba), Z (Profundidad, negativo es hacia adelante)
            const float posiciones[10][3] = {
                    { 0.0f,  0.0f, -3.0f}, // 1ra: Centro exacto (no se usa acá, se pone al iniciar)
                    { 0.8f,  0.2f, -3.2f}, // 2da: Un poco corrida de la primera hacia la derecha y arriba
                    {-1.2f, -0.4f, -2.8f}, // 3ra: Abajo a la izquierda
                    { 1.5f,  0.5f, -3.5f}, // 4ta: Arriba a la derecha (exige un poco más)
                    {-0.5f,  0.8f, -3.0f}, // 5ta: Arriba al centro-izquierda
                    { 0.0f,  0.0f, -5.5f}, // 6ta: Bien centrada pero mucho MÁS LEJOS (-5.5 en Z)
                    {-1.8f, -0.2f, -3.0f}, // 7ma: Izquierda exigida
                    { 1.0f, -0.6f, -2.5f}, // 8va: Abajo a la derecha
                    {-0.4f,  0.2f, -3.0f}, // 9na: Cerca, cómoda y casi centrada (para persona mayor)
                    { 0.4f, -0.1f, -2.8f}  // 10ma: Cerca, cómoda y centrada del otro lado
            };

            // Tomamos la posición correspondiente según el puntaje actual
            g_current_target_pos = {posiciones[g_score][0], posiciones[g_score][1], posiciones[g_score][2]};

            // Reiniciamos los valores de la animación
            g_current_scale = 1.0f;
            g_current_angle = 0.0f;

        } else if (g_game_state == ESPERANDO_REINICIO) {
            // Si mira el botón de reinicio, reseteamos todo a cero
            g_score = 0;
            g_current_target_pos = {0.0f, 0.0f, -3.0f}; // Posición inicial
            g_game_state = JUGANDO;
        }
    }

bool HelloCardboardApp::IsPointingAtTarget() {
  // Compute vectors pointing towards the reticle and towards the target object
  // in head space.
  Matrix4x4 head_from_target = head_view_ * model_target_;

  const std::array<float, 4> unit_quaternion = {0.f, 0.f, 0.f, 1.f};
  const std::array<float, 4> point_vector = {0.f, 0.f, -1.f, 0.f};
  const std::array<float, 4> target_vector = head_from_target * unit_quaternion;

  float angle = AngleBetweenVectors(point_vector, target_vector);
  return angle < kAngleLimit;
}

}  // namespace ndk_hello_cardboard
