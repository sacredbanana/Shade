#version 410 core
#define PI 3.1415926535
#define MAX_ANGLE 6.*PI

uniform float fGlobalTime; // in seconds
uniform vec2 v2Resolution; // viewport resolution (in pixels)

uniform sampler1D texFFT; // towards 0.0 is bass / lower freq, towards 1.0 is higher / treble freq
uniform sampler1D texFFTSmoothed; // this one has longer falloff and less harsh transients
uniform sampler1D texFFTIntegrated; // this is continually increasing
uniform sampler2D texNoise;
uniform sampler2D texChecker;


#define time fGlobalTime
#define iTime fGlobalTime

layout(location = 0) out vec4 out_color; // out_color must be written in order to see anything

#define MIN_DIST .005
#define MAX_STEPS 300

float height(vec3 ro) {return texture(texNoise,vec2(ro.x/100.,ro.z/100.)).x*15.;}

vec2 plane(vec3 ro, vec3 pos) {
    float h = height(ro);
    h += texture(texNoise,vec2(ro.x/.2,ro.z/.2)).x/2.;
 	return vec2(ro.y-pos.y-h, abs(sin(ro.x/2.)+cos(ro.z/2.)));   
}

mat2 rot2d(float a) {
  return mat2(cos(a), sin(a), -sin(a), cos(a));
}

void main(void)
{
    vec2 uv = gl_FragCoord.xy/v2Resolution.xy;
    uv -= .5;
    uv *= 2.;
    uv.x *= v2Resolution.x/v2Resolution.y;
    
    float speed = 2.;
    float zoom = 2.;
    float angle = 0.;
    vec3 up = vec3(sin(angle),cos(angle),0.);
    vec3 ro = vec3(2.*sin(.2*iTime),4.1,speed*iTime);
    ro.y = height(ro)+.3;
    float heightFront = height(vec3(ro.x,ro.y,ro.z+.75));
    vec3 target = vec3(4.*sin(.1+.2*iTime)/heightFront,heightFront,ro.z+1.);
    vec3 f = normalize(target-ro);
    vec3 r = cross(up,f);
    vec3 u = cross(f,r);
    vec3 c = ro + f*zoom;
    vec3 camera = c + uv.x*r + uv.y*u;
    vec3 rd = normalize(camera-ro);
    vec3 ray = ro;
    
    // Sky
    vec3 col = vec3(0.,.6,1.)+texture(texNoise,vec2((uv.x+camera.x*2.)/6.,uv.y+camera.y*2.)).rgb;
    for (int i = 0; i < MAX_STEPS; i++) {
        vec2 d = plane(ray, vec3(0.,0.,0.));
        if (d.x < MIN_DIST) {
          float light = .3/pow(distance(ray,ro)/5.+.1,1.6)*distance(ray,ro)*.1;
          light += .1/pow(distance(ray,ro)+.1,2.6);
          col = vec3(.14*light+.01,.6*light+.04,.13*light+.01)*2.;
          break;
        }
        
        ray += rd*d.x;
    }
    
    // Output to screen
    out_color = vec4(col,1.0);
}