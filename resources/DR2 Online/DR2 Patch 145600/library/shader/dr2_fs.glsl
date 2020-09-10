#version 400

layout (location = 0) out vec4 FragColour;

in vec4 diffuse;
in vec4 specular;
in vec2 TexCoord;
in vec4 gl_FragCoord;
in vec4 pos;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;

uniform vec4 v4ambient = vec4(1.0f, 1.0f, 1.0f, 1.0f);
uniform vec4 v4textureFactor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
uniform float alphaMix = 0.0f;
uniform bool isTranslucent = true;
uniform bool doTranslucent = true;

uniform bool doFog = true;
uniform vec4 fogColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);
uniform float renderState_fogMin = 0.0f;
uniform float renderState_fogMax = 0.0f;
uniform float renderState_fogDensity = 0.0f;
uniform float renderState_fogFactor = 600.0f;
uniform float renderState_fogFactorZ = 0.0f;

uniform bool hasTexture0 = true;
uniform bool doTexture = true;
uniform bool doStaged = false;
uniform bool doParticles = false;
uniform bool doTerrain = false;
uniform bool doObj = false;
uniform bool doClient = false;

uniform bool doInterface = false;
uniform bool doInterfaceTextured = false;
uniform bool doInterfaceTranslucent = false;

uniform bool doBlendMask = false;
uniform bool doBuckyMask = false;
uniform bool doBlendGlow = false;
uniform bool doBlendAdd = false;
uniform bool doBlendModulate = false;
uniform bool doBlendModulate2 = false;
uniform bool doBlendModulate4 = false;
uniform bool doBlendDecal = false;
uniform bool doBlendDecalAlpha = false;
uniform bool doBlendDefault = false;

// DEBUGGING
vec3 pink3 = vec3(1.0f, 0.0f, 1.0f);
vec3 red3 = vec3(1.0f, 0.0f, 0.0f);
vec3 green3 = vec3(0.0f, 1.0f, 0.0f);
vec3 blue3 = vec3(0.0f, 0.0f, 1.0f);

vec4 pink4 = vec4(1.0f, 0.0f, 1.0f, 1.0f);
vec4 red4 = vec4(1.0f, 0.0f, 0.0f, 1.0f);
vec4 green4 = vec4(0.0f, 1.0f, 0.0f, 1.0f);
vec4 blue4 = vec4(0.0f, 0.0f, 1.0f, 1.0f);

float Fog() {
      vec4 coord = gl_FragCoord / gl_FragCoord.w;
      float d = 1 - coord.z;
      float fog = clamp(1.0 / exp(d * renderState_fogFactor), 0.0f, 1.0f);
      
      return 1 - fog;
}

vec4 apply_fog(vec4 col) {
      vec4 newcol = col;

      if (doFog) {
            newcol.rgb += fogColor.rgb * (1 - Fog());
      }

      return newcol;
}

// gl_FragCoord.z
// 0 nearest cam
// 1 furthest cam
// 1 - gl_FragCoord.z -- invert coordinates

void main() {
      vec4 vcol = diffuse.bgra / 255.0f;

      float z = 1;

      vec4 tex1 = texture2D(texture1, vec2(TexCoord.x * z, TexCoord.y * z));

      vec4 tex2 = texture(texture2, TexCoord);
      vec4 tex3 = texture(texture3, TexCoord);
      FragColour = vcol;

      if (doParticles) {
            FragColour = green4;
            return;
      }

      if (doInterface) {
            if (doInterfaceTextured && !doInterfaceTranslucent) {
                  FragColour.a = 1.0f;
                  FragColour.rgb = vcol.rgb * tex1.rgb;
            }
            // General interface.
            if (doInterfaceTextured && doInterfaceTranslucent) {
                  FragColour.a = vcol.a * tex1.a;
                  FragColour.rgb = vcol.rgb * tex1.rgb;
            }
            if (!doInterfaceTextured) {
                  FragColour = vcol;
            }

            return;
      }
      
      if (doObj) {
            if (!doStaged) {
                  if (doTranslucent) {
                        FragColour.rgb = tex1.rgb;
                        FragColour.a = vcol.a;
                  }

                  if (doBlendModulate) {
                        FragColour.rgb = vcol.rgb * tex1.rgb;
                        FragColour.a = vcol.a;
                  }
            }

            if (doStaged) {
                  if (doTranslucent) {
                        FragColour.rgb = tex1.rgb;
                        FragColour.a = vcol.a;
                  }

                  if (doBlendModulate) {
                        FragColour *= vcol + tex1;
                        FragColour.a = vcol.a;
                  }
            }

            FragColour = apply_fog(FragColour);
            return;
      }

      if (doTerrain) {
            if (doBlendAdd) {
                  // Sun / moon. (Maybe blown up enforcers?)
                  FragColour.rgb = tex1.rgb * vcol.rgb;
                  FragColour.a = tex1.a;
                  FragColour = apply_fog(FragColour);
                  return;
            }

            // Terrain textures don't store alpha channel.
            // Terrain textures rely on vcol.
            FragColour.rgb = vcol.rgb * tex1.rgb * vcol.a;
            FragColour = apply_fog(FragColour);
            return;
      }

      // Shadows, decals, translucent buildings, taelon, light beams from zeplin.
      if (doClient) {
            // Sky.
            if (doTranslucent && doBlendAdd) {
                  FragColour = tex1;
                  FragColour = apply_fog(FragColour);
                  return;
            }

            // Smoke, particles, fence links.
            if (doTranslucent && doBlendGlow && !doBlendModulate) {
                  FragColour = tex1 * vcol;

                  // if (TexCoord.x > 1 || TexCoord.x < 0) {
                  //       discard;
                  //       return;
                  // }

                  // if (TexCoord.y > 1 || TexCoord.y < 0) {
                  //       discard;
                  //       return;
                  // }

                  //return;
            }

            if (doTranslucent && doBlendGlow && doBlendModulate) {
                  FragColour = blue4;
                  //return;
            }
            
            if (doBlendGlow && !doBlendModulate) {
                  // Taelon glow is encoded in vcol.rgb.
                  FragColour.rgb *= tex1.rgb;
                  FragColour.a *= tex1.a;

                  // Light beams.
                  if (!doTranslucent) {
                        FragColour.a = vcol.a;
                  }

                  // ??
                  //if (doTranslucent) {
                  //      FragColour = red4;
                  //}
                  
                  //return;
            }

            if (!doTranslucent) {
                  FragColour = tex1 * vcol;
                  FragColour.a = vcol.a;
                  //return;
            }

            if (doTranslucent && doBlendDecal && doBlendModulate && !doBlendGlow) {
                  //FragColour = tex1;
                  //FragColour.a = mix(vcol.a, 1, tex1.a);
                  FragColour.a = vcol.a;
                  FragColour.rgb = tex1.rgb * vcol.rgb;

                  // Building decal, taelon transparent.
                  if (doStaged) {
                        //FragColour = pink4 * vcol.a;
                        //return;
                  }

                  // Grid colours (when building fences etc.)
                  if (!doStaged) {
                        FragColour = vcol * tex1;
                        //return;
                  }
                  
                  //FragColour = pink4;
                  //return;
            }

            FragColour = apply_fog(FragColour);
            return;
      }

      FragColour *= tex1;
};
