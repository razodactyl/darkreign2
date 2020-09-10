///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// lightvertsmodel.cpp
//
// 18-SEP-1998
//

#include "vid_public.h"
#include "light_priv.h"
#include "mesh.h"
#include "perfstats.h"

//-----------------------------------------------------------------------------

namespace Vid
{
    namespace Light
    {
        void SetupLightsModel()
        {
            PERF_X_S("light");

            NList<Obj>::Iterator li(&activeList);
            while (Obj* light = li++)
            {
#ifdef DOSPECULAR
        light->doSpecular = Vid::renderState.status.specular && (light->d3d.dwFlags & D3DLIGHT_NO_SPECULAR) != D3DLIGHT_NO_SPECULAR ? TRUE : FALSE;
#endif

                switch (light->GetType())
                {
                    case lightPOINT:
                    {
                        // calculate the light's model space position and direction
                        Vid::Math::worldMatrixInv.Transform(light->position, light->WorldMatrix().Position());

#ifdef DOSPECULAR
            light->direction = light->position;
            light->direction.Normalize();

	          // calculate the halfway vector using the D3D method
	          light->halfVector = (Vid::Math::modelViewNorm + light->direction);
  	        light->halfVector.Normalize();
#endif
                        break;
                    }

                    case lightSPOT:
                    {
                        // calculate the model space position and direction
                        Vid::Math::worldMatrixInv.Transform(light->position, light->WorldMatrix().Position());

#ifdef DOSPECULAR
            light->direction = light->position;
            light->direction.Normalize();

    	    // calculate the halfway vector using the D3D method
      	    light->halfVector = (Vid::Math::modelViewNorm + light->direction);
	          light->halfVector.Normalize();
#endif
                        Vid::Math::worldMatrixInv.Rotate(light->direction, light->WorldMatrix().Front());
                        // align with normals
                        light->direction *= -1.0f;
                    }
                    break;

                    case lightDIRECTION:
                    {
                        // calculate the vector to the light in model space
                        Vid::Math::worldMatrixInv.Rotate(light->direction, light->WorldMatrix().Front());
                        // align with normals
                        light->direction *= -1.0f;

#ifdef DOSPECULAR
          // calculate the halfVector vector using the D3D method
      	    light->halfVector = (Vid::Math::modelViewNorm + light->direction);
      	    light->halfVector.Normalize();
#endif
                        break;
                    }
                }
#ifdef __DO_XMM_BUILD
        if (Vid::isStatus.xmm)
        {
          SetupLightModelXmm(*light);
        }
#endif
            }
            PERF_X_E("light");
        }

        //-----------------------------------------------------------------------------

        void LightModel(const Vector& vert, const Vector& norm, const ColorF32 diffIn, const Material& material, ColorF32& diff, ColorF32& spec)
        {
            PERF_X_S("light");

            spec.Set(0, 0, 0);

            NList<Obj>::Iterator li(&activeList);
            while (Obj* light = li++)
            {
                switch (light->GetType())
                {
                    case lightPOINT:
                        light->PointLightModel(vert, norm, diffIn, material, diff, spec);
                        break;
                    case lightSPOT:
                        light->SpotLightModel(vert, norm, diffIn, material, diff, spec);
                        break;
                    case lightDIRECTION:
                        light->DirectLightModel(vert, norm, diffIn, material, diff, spec);
                        break;
                }
            }
            PERF_X_E("light");
        }

        //----------------------------------------------------------------------------

        void Obj::PointLightModel(const Vector& vert, const Vector& norm, const ColorF32& diffIn, const Material& material, ColorF32& diff, ColorF32& spec)
        {
            spec;
            material;

#ifdef DOSPECULAR
      Bool dospec = material.GetStatus().specular && doSpecular;
#endif

            // calculate distance from the vertex to the light
            Vector dir;
            dir = position - vert;
            F32 dist = dir.Magnitude();

            // if distance is greater than range, then no light reaches the vertex
            if (dist <= d3d.dvRange)
            {
                if (dist == 0.0f)
                {
                    dist = F32_EPSILON;
                }
                F32 invdist = 1.0f / dist;

                // normalize the distance
                dist = (d3d.dvRange - dist) * invRange;

                // calculate the attenuation over that distance
                F32 a = d3d.dvAttenuation0 + dist * d3d.dvAttenuation1 + (dist * dist) * d3d.dvAttenuation2;

                if (a > 0.0f)
                {
                    // normalize the direction vector
                    dir *= invdist;

                    // calculate and scale the attenuated color values
                    ColorF32 atten;
                    atten.r = a * d3d.dcvDiffuse.r;
                    atten.g = a * d3d.dcvDiffuse.g;
                    atten.b = a * d3d.dcvDiffuse.b;

                    // calculate the diffuse reflection factor and clamp it to [0,1]
                    // NOTE: both src->nv and light->dvDirection must be normalized vectors
                    F32 diffuse_reflect = norm.Dot(dir); // -1.0f <= diffuse_reflect <= 1.0f

                    if (diffuse_reflect > 0.0f)
                    {
                        // calculate the diffuse component for the vertex
                        diff.r += atten.r * diffuse_reflect * diffIn.r;
                        diff.g += atten.g * diffuse_reflect * diffIn.g;
                        diff.b += atten.b * diffuse_reflect * diffIn.b;
                    }

#ifdef DOSPECULAR
			    if (dospec)
			    {
				    // calculate the partial specular reflection factor
				    F32 spec_reflect = norm.Dot(halfVector);
				    
				    if ( spec_reflect > SPECULAR_THRESHOLD )
				    {
    //					spec_reflect = (F32) pow((double) spec_reflect, (double) material.GetDesc().dvPower); // -1.0f <= spec_reflect <= 1.0f
              // apply the material's power factor to the nearest power of 2
              U32 e, ee = material.PowerCount();
              for (e = 1; e < ee; e *= 2 )
              {
                spec_reflect = spec_reflect * spec_reflect; 
              }

              // calculate the diffuse component of the vertex
					    // if vertex color capabilities are added this must be changed
              spec.r += atten.r * spec_reflect * material.GetDesc().specular.r;
              spec.g += atten.g * spec_reflect * material.GetDesc().specular.g;
              spec.b += atten.b * spec_reflect * material.GetDesc().specular.b;
				    }
			    }
#endif
                } // if
            } // if
        }

        //----------------------------------------------------------------------------

        void Obj::SpotLightModel(const Vector& vert, const Vector& norm, const ColorF32& diffIn, const Material& material, ColorF32& diff, ColorF32& spec)
        {
            spec;
            material;

#ifdef DOSPECULAR
      Bool dospec = material.GetStatus().specular && doSpecular;
#endif

            // calculate distance from the light to the vertex 
            Vector dir;
            dir = position - vert;
            F32 dist = dir.Magnitude();

            // if distance is greater than range, then no light reaches the vertex
            if (dist <= d3d.dvRange)
            {
                if (dist == 0.0f)
                {
                    dist = F32_EPSILON;
                }
                F32 invdist = 1.0f / dist;

                // normalize the distance
                dist = (d3d.dvRange - dist) * invRange;

                // calculate the attenuation over that distance
                F32 a = d3d.dvAttenuation0 + dist * d3d.dvAttenuation1 + (dist * dist) * d3d.dvAttenuation2;

                if (a > 0.0f)
                {
                    // compute the cosine of vectors vert_to_light and the light's model space direction
                    dir *= invdist;
                    F32 cos_dir = dir.Dot(direction);

                    if (cos_dir > cosPhi)
                    {
                        F32 intensity;

                        if (cos_dir > cosTheta) // vertex is inside inner cone --> receives full light
                        {
                            intensity = 1.0f;
                        }
                        else // vertex is between inner and outer cone
                        {
                            //						intensity = (F32) pow((double) ((cos_dir-cos_phi)/(cos_theta-cos_phi)), (double) d3d.dvFalloff);
                            intensity = (cos_dir - cosPhi) * invAngle;
                        }
                        // calculate and scale the attenuated color values
                        ColorF32 atten;
                        atten.r = a * d3d.dcvDiffuse.r;
                        atten.g = a * d3d.dcvDiffuse.g;
                        atten.b = a * d3d.dcvDiffuse.b;

                        // calculate the diffuse reflection factor and clamp it to [0,1]
                        // NOTE: both src->nv and light->dvDirection must be normalized vectors
                        F32 diffuse_reflect = norm.Dot(dir); // -1.0f <= diffuse_reflect <= 1.0f

                        if (diffuse_reflect > 0.0f)
                        {
                            diffuse_reflect *= intensity;

                            // calculate the diffuse component for the vertex
                            diff.r += atten.r * diffuse_reflect * diffIn.r;
                            diff.g += atten.g * diffuse_reflect * diffIn.g;
                            diff.b += atten.b * diffuse_reflect * diffIn.b;
                        }

#ifdef DOSPECULAR
				    if ( dospec )
				    {
					    // calculate the partial specular reflection factor
					    F32 spec_reflect = norm.Dot(halfVector);
					    
  				    if ( spec_reflect > SPECULAR_THRESHOLD )
					    {
    //  					spec_reflect = (F32) pow((double) spec_reflect, (double) material.GetDesc().dvPower); // -1.0f <= spec_reflect <= 1.0f
                // apply the material's power factor to the nearest power of 2
                U32 e, ee = material.PowerCount();
                for (e = 1; e < ee; e *= 2 )
                {
                  spec_reflect = spec_reflect * spec_reflect; 
                }

						    spec_reflect *= intensity;

                spec.r += atten.r * spec_reflect * material.GetDesc().specular.r;
                spec.g += atten.g * spec_reflect * material.GetDesc().specular.g;
                spec.b += atten.b * spec_reflect * material.GetDesc().specular.b;
					    }
				    }
#endif
                    } // if
                } // if
            } // if
        }

        //----------------------------------------------------------------------------

        void Obj::DirectLightModel(const Vector& vert, const Vector& norm, const ColorF32& diffIn, const Material& material, ColorF32& diff, ColorF32& spec)
        {
            vert;
            spec;
            material;

#ifdef DOSPECULAR
      Bool dospec = material.GetStatus().specular && doSpecular;
#endif

            F32 diffuse_reflect = norm.Dot(direction); // -1.0f <= diffuse_reflect <= 1.0f

            if (diffuse_reflect > 0.0f)
            {
                // calculate the diffuse component for the vertex
                diff.r += d3d.dcvDiffuse.r * diffuse_reflect * diffIn.r;
                diff.g += d3d.dcvDiffuse.g * diffuse_reflect * diffIn.g;
                diff.b += d3d.dcvDiffuse.b * diffuse_reflect * diffIn.b;
            }

#ifdef DOSPECULAR
	    if (dospec)
	    {
		    // calculate the partial specular reflection factor
		    F32 spec_reflect = norm.Dot(halfVector);

  	    if ( spec_reflect > SPECULAR_THRESHOLD )
		    {
    //			spec_reflect = (F32) pow((double) spec_reflect, (double) material.GetDesc().dvPower); // -1.0f <= spec_reflect <= 1.0f
          // apply the material's power factor to the nearest power of 2
          U32 e, ee = material.PowerCount();
          for (e = 1; e < ee; e *= 2 )
          {
            spec_reflect = spec_reflect * spec_reflect; 
          }

  		    // calculate the diffuse component of the vertex
			    // if vertex color capabilities are added this must be changed
          spec.r += d3d.dcvDiffuse.r * spec_reflect * material.GetDesc().specular.r;
          spec.g += d3d.dcvDiffuse.g * spec_reflect * material.GetDesc().specular.g;
          spec.b += d3d.dcvDiffuse.b * spec_reflect * material.GetDesc().specular.b;
		    }
	    }
#endif
        }

        //----------------------------------------------------------------------------

        void SetupLightsModelQuick()
        {
            NList<Obj>::Iterator li(&activeList);
            while (Obj* light = li++)
            {
                switch (light->GetType())
                {
                    case lightPOINT:
                        // calculate the light's model space position and direction
                        Vid::Math::worldMatrixInv.Transform(light->position, light->WorldMatrix().Position());
                        break;

                    case lightSPOT:
                    {
                        // calculate the model space position and direction
                        Vid::Math::worldMatrixInv.Transform(light->position, light->WorldMatrix().Position());

                        light->direction = light->position;
                        light->direction.Normalize();
                    }
                    break;
                }
#ifdef __DO_XMM_BUILD
        if (Vid::isStatus.xmm)
        {
          SetupLightModelXmm(*light);
        }
#endif
            }
        }

        //-----------------------------------------------------------------------------

        void LightModelQuick(const Vector& vert, ColorF32& diff)
        {
            NList<Obj>::Iterator li(&activeList);
            while (Obj* light = li++)
            {
                switch (light->GetType())
                {
                    case lightPOINT:
                        light->PointLightModel(vert, diff);
                        break;
                    case lightSPOT:
                        light->SpotLightModel(vert, diff);
                        break;
                    case lightDIRECTION:
                        // calculate the diffuse component for the vertex
                        diff.r += light->R();
                        diff.g += light->G();
                        diff.b += light->B();
                        break;
                }
            }
        }

        //----------------------------------------------------------------------------

        void Obj::PointLightModel(const Vector& vert, ColorF32& diff)
        {
            // calculate distance from the vertex to the light
            Vector dir;
            dir = position - vert;
            F32 dist = dir.Magnitude();

            // if distance is greater than range, then no light reaches the vertex
            if (dist <= d3d.dvRange)
            {
                if (dist == 0.0f)
                {
                    dist = F32_EPSILON;
                }
                F32 invdist = 1.0f / dist;

                // normalize the distance
                dist = (d3d.dvRange - dist) * invRange;

                // calculate the attenuation over that distance
                F32 a = d3d.dvAttenuation0 + dist * d3d.dvAttenuation1 + (dist * dist) * d3d.dvAttenuation2;

                if (a > 0.0f)
                {
                    // normalize the direction vector
                    dir *= invdist;

                    // calculate and scale the attenuated color values
                    ColorF32 atten;
                    atten.r = a * d3d.dcvDiffuse.r;
                    atten.g = a * d3d.dcvDiffuse.g;
                    atten.b = a * d3d.dcvDiffuse.b;

                    // calculate the diffuse component for the vertex
                    diff.r += atten.r;
                    diff.g += atten.g;
                    diff.b += atten.b;
                } // if
            } // if
        }

        //----------------------------------------------------------------------------

        void Obj::SpotLightModel(const Vector& vert, ColorF32& diff)
        {
            // calculate distance from the light to the vertex 
            Vector dir;
            dir = position - vert;
            F32 dist = dir.Magnitude();

            // if distance is greater than range, then no light reaches the vertex
            if (dist <= d3d.dvRange)
            {
                if (dist == 0.0f)
                {
                    dist = F32_EPSILON;
                }
                F32 invdist = 1.0f / dist;

                // normalize the distance
                dist = (d3d.dvRange - dist) * invRange;

                // calculate the attenuation over that distance
                F32 a = d3d.dvAttenuation0 + dist * d3d.dvAttenuation1 + (dist * dist) * d3d.dvAttenuation2;

                if (a > 0.0f)
                {
                    // compute the cosine of vectors vert_to_light and the light's model space direction
                    dir *= invdist;
                    F32 cos_dir = dir.Dot(direction);

                    if (cos_dir > cosPhi)
                    {
                        F32 intensity;

                        if (cos_dir > cosTheta) // vertex is inside inner cone --> receives full light
                        {
                            intensity = 1.0f;
                        }
                        else // vertex is between inner and outer cone
                        {
                            //						intensity = (F32) pow((double) ((cos_dir-cos_phi)/(cos_theta-cos_phi)), (double) d3d.dvFalloff);
                            intensity = (cos_dir - cosPhi) * invAngle;
                        }
                        // calculate and scale the attenuated color values
                        ColorF32 atten;
                        atten.r = a * intensity * d3d.dcvDiffuse.r;
                        atten.g = a * intensity * d3d.dcvDiffuse.g;
                        atten.b = a * intensity * d3d.dcvDiffuse.b;

                        // calculate the diffuse component for the vertex
                        diff.r += atten.r;
                        diff.g += atten.g;
                        diff.b += atten.b;
                    } // if
                } // if
            } // if
        }

        //----------------------------------------------------------------------------
    };
};
