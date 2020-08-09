#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gDiffuseColor;
uniform sampler2D gMask;
uniform sampler2D gDepth;
uniform sampler2D gShadowMask;
uniform sampler2D gShadowDepth;
uniform sampler2D gShadowMask1;
uniform sampler2D gShadowDepth1;

float depth;
vec3 vPosition;
vec3 vTransformedNormal;
vec4 vDiffuseColor;
vec4 vPosLightSpace;
vec4 vPosLightSpace1;
float mask;

uniform int uUseLighting;
uniform int uShowDepth;
uniform int uShowNormals;
uniform int uShowPosition;
uniform int uPerspectiveProjection;
uniform int uUseShadow; 

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform mat4 uInvVMatrix;
uniform mat4 uInvPMatrix;
uniform mat3 uNMatrix;
uniform mat4 lightSpaceMatrix;
uniform mat4 lightSpaceMatrix1;

uniform vec3 uAmbientColor;
uniform vec3 uPointLightingLocation;
uniform vec3 uPointLightingColor;
uniform vec3 uPointLightingLocation1;
uniform vec3 uPointLightingColor1;

vec3 ViewPosFromDepth(float depth){
	float z = depth * 2.0 - 1.0;

	vec4 clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4 viewSpacePosition = uInvPMatrix * clipSpacePosition;

	// Perspective division
    viewSpacePosition /= viewSpacePosition.w;

	return viewSpacePosition.xyz;
}

float ShadowCalculation(vec4 fragPosLightSpace, sampler2D gShadowMask, sampler2D shadowMap, vec3 lightDir){
	// perform perspective divide
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	// transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
	// get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
	float closestDepth = texture(shadowMap, projCoords.xy).r;
	if (mask < 0.5)
		closestDepth = 1.0;
	// get depth of current fragment from light's perspective
	float currentDepth = projCoords.z;
	// calculate bias (based on depth map resolution and slope)
	vec3 normal = normalize(vTransformedNormal);
	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	// check whether current frag pos is in shadow
	float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
	// PCF
	/*float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
	for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;*/

	// keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
		shadow = 0.0;

	return shadow;
}

void main()
{
	// backgroundColor
    vec4 bgColor = vec4(1.0, 1.0, 1.0, 0.0);

	// retrive data from gbuffer
	// vec3 vPosition = (uMVMatrix * vec4( texture(gPosition, TexCoords).rgb, 1.0 )).xyz;
	depth = texture(gDepth, TexCoords).r;
	vPosition = ViewPosFromDepth(depth);
	vTransformedNormal = texture(gNormal, TexCoords).rgb;
	vDiffuseColor = texture(gDiffuseColor, TexCoords).rgba;
	mask = texture(gMask, TexCoords).r;
	vPosLightSpace = lightSpaceMatrix * uInvVMatrix * vec4(vPosition, 1.0);
	vPosLightSpace1 = lightSpaceMatrix1 * uInvVMatrix * vec4(vPosition, 1.0);

	FragColor = vec4(TexCoords * 2.0 - 1.0, depth * 2 - 1, 1.0);

	/*// calculate lighting as usual 
	vec3 ambient = vDiffuseColor.rgb * uAmbientColor;
	vec3 color = ambient;

	if (uUseLighting){
		vec3 light_direction = normalize(uPointLightingLocation - vPosition.xyz);
		vec3 light_direction1 = normalize(uPointLightingLocation1 - vPosition.xyz);
		vec3 eye_direction = normalize(-vPosition.xyz);
		vec3 surface_normal = normalize(vTransformedNormal);

		vec3 diffuse = vDiffuseColor.xyz * uPointLightingColor * max(dot(surface_normal, light_direction), 0.0);
		vec3 diffuse1 = vDiffuseColor.xyz * uPointLightingColor1 * max(dot(surface_normal, light_direction1), 0.0);
		vec3 specular  = uPointLightingColor  * pow( max( dot( reflect( -light_direction,  surface_normal ), eye_direction ), 0.0 ), 100.0 );
		vec3 specular1 = uPointLightingColor1 * pow( max( dot( reflect( -light_direction1, surface_normal ), eye_direction ), 0.0 ), 100.0 );
		float light_outer_radius  = 20.0;
		float light_inner_radius  = 0.0;
		float light_outer_radius1 = 20.0;
		float light_inner_radius1 = 0.0;
		float light_distance  = length( vPosition.xyz - uPointLightingLocation  );
		float light_distance1 = length( vPosition.xyz - uPointLightingLocation1 );
		float attenuation     = 1.0 - smoothstep( light_inner_radius,  light_outer_radius,  light_distance  );
		float attenuation1    = 1.0 - smoothstep( light_inner_radius1, light_outer_radius1, light_distance1 );
		diffuse   = attenuation  * diffuse;
		diffuse1  = attenuation1 * diffuse1;
		specular  = attenuation  * specular;
		specular1 = attenuation1 * specular1;
		if (uUseShadow){
			// calculate shadow 
			float shadow = ShadowCalculation(vPosLightSpace, gShadowMask, gShadowDepth, light_direction); 
			float shadow1 = ShadowCalculation(vPosLightSpace1, gShadowMask1, gShadowDepth1, light_direction1); 
			color = ambient + (1.0 - shadow) * (diffuse + specular) + (1.0 - shadow1) * (diffuse1 + specular1);
			//color = vec3(1 - shadow1);
		} else {
			color = ambient + diffuse + diffuse1 + specular + specular1;
		}
	}
	//FragColor = mask * vec4(color, 1.0);
	FragColor = mask * vec4(color, vDiffuseColor.a) + (1 - mask) * bgColor;

	if ( uShowDepth ) {
		// FragColor = mix( vec4( 1.0 ), vec4( vec3( 0.0 ), 1.0 ), smoothstep( 0.1, 1.0, fog_coord ) );
		//FragColor = vDiffuseColor;
		//float shadowDepth = texture(gShadowDepth1, TexCoords).r;
		//FragColor = vec4( vec3(shadowDepth), 1.0 );
		//float shadowMask = texture(gShadowMask1, TexCoords).r;
		FragColor = vec4( vec3(mask), 1.0 );
	}
	if (uShowNormals) {
		vec3 nTN      = normalize(vTransformedNormal);
		FragColor = vec4(nTN, 1.0);
		
		//FragColor = vec4(vec3((length(vTransformedNormal))), 1.0);
		//if (mask == 0)
		//	FragColor = vec4(1.0);
	}
	if (uShowPosition) {
		//vec3 nP       = vPosition.xyz;
		vec3 nP = vPosLightSpace.xyz;
		FragColor  = mask * vec4(nP , 1.0);
	}*/
}
