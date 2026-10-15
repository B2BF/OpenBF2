// Emitter-particle transforms, collision history and child-system advancement.
// Each 176-byte record owns one child pointer and supplies its parent transform.

void psmType3Emit(void* self, float dt);

// a .data float, 0x3C23D70A, with four readers (all in this class) and no writer - the
// fixed simulation step. Kept as its own constant rather than the 0.01f literal the siblings use.
const float psmType3TimeStep = 0.01f;

// PE 101817A0, 74 B: byte-for-byte the ordinary 1017F210 shape with the detached byte at +0x18
// (type 2's offset) and no water arm. Mat4::mult is the tree's recovered 10077C50.
void psmType3RefreshTransform(void* self)
{
    if (psmInstanceB<unsigned char>(self,0x18)) return;
    Mat4& output=psmInstanceB<Mat4>(self,0x74);
    const Mat4& local=psmInstanceB<Mat4>(self,0x34);
    world::IObject* const owner=psmInstanceB<world::IObject*>(self,0x10);
    if (owner) output.mult(local,owner->getVisualAbsoluteTransformation());
    else if (psmInstanceB<const Mat4*>(self,0x30)) output.mult(local,*psmInstanceB<const Mat4*>(self,0x30));
    else output=local;
}

// PE 10182750, 146 B: every third of a second the current plane (+0x200) is copied into the ten-entry
// history (+0x160 + 16*i), normalised there, and its distance re-attached.
// 1018275D `test ah,5; jnp`: only an ordered less-than skips, so a NaN accumulator runs the body.
void psmType3RememberPlane(void* self)
{
    if (!(psmInstanceB<float>(self,0x214)<0.33333001f))
    {
        const unsigned char next=psmInstanceB<unsigned char>(self,0x211);
        psmInstanceB<unsigned char>(self,0x210)=next;
        psmInstanceB<float>(self,0x214)=static_cast<float>(
            static_cast<double>(psmInstanceB<float>(self,0x214))-0.33333001f);
        const float distance=psmInstanceB<float>(self,0x20C);          // loaded before the copy
        float* const target=&psmInstanceB<float>(self,0x160+16u*next);
        const float* const source=&psmInstanceB<float>(self,0x200);
        target[0]=source[0]; target[1]=source[1]; target[2]=source[2];
        psmNormalize3(target);
        target[3]=distance;
        // The byte is reloaded after the normalise and wrapped with a signed idiv (always positive).
        psmInstanceB<unsigned char>(self,0x211)=static_cast<unsigned char>(
            (static_cast<int>(psmInstanceB<unsigned char>(self,0x211))+1)%10);
    }
}

// PE 101744A0, 106 B: rows right/up/forward with zero fourth columns, translation row and m[15]=1.
void psmType3SetBasis(float* matrix, const float* right, const float* up, const float* forward,
                      const float* position)
{
    matrix[0]=right[0]; matrix[1]=right[1]; matrix[2]=right[2]; matrix[3]=0;
    matrix[4]=up[0]; matrix[5]=up[1]; matrix[6]=up[2]; matrix[7]=0;
    matrix[8]=forward[0]; matrix[9]=forward[1]; matrix[10]=forward[2]; matrix[11]=0;
    matrix[12]=position[0]; matrix[13]=position[1]; matrix[15]=1; matrix[14]=position[2];
}

// PE 10181820, 2277 B (`retn 10h`): advance one emitter particle by `steps` fixed steps of `step`.
// Returns 0 when the particle expired (the caller keeps the same index), 1 otherwise.
int psmType3AdvanceParticle(void* self, unsigned char* record, int index, float step, unsigned int steps)
{
    ParticleSystemManager* const manager=reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager);
    // `fild` plus the 2^32 correction: steps is unsigned. The float copy (var_4) feeds the rotation.
    const double stepsExact=static_cast<double>(steps);
    const float stepsF=static_cast<float>(stepsExact);
    const float span=static_cast<float>(stepsExact*step);                // var_8, also the child's dt
    // Unlike types 0/2/4 the age is NOT advanced before this test; it only advances inside the
    // step loop below. 10181855 `test ah,1; jnz`: less-than OR unordered keeps the particle alive.
    if (static_cast<double>(span)+psmRuntimeField<float>(record,0x7C)>=psmRuntimeField<float>(record,0x80))
    {
        if (psmRecordPointer<IParticleSystemInstance*>(record, 3))
        {
            // Hand the child back to the manager: byte +5 = 0 lets the bucket walk update it again,
            // removeInstance detaches+stops it (live particles) or queues its destruction (none).
            psmInstance<unsigned char>(psmRecordPointer<IParticleSystemInstance*>(record, 3),5)=0;
            reinterpret_cast<IParticleSystemManager*>(g_ParticleSystemManager)
                ->removeInstance(psmRecordPointer<IParticleSystemInstance*>(record, 3));
        }
        --psmInstanceB<unsigned int>(self,0x28);
        psmInstanceB<unsigned char**>(self,0x14)[index]=
            psmInstanceB<unsigned char**>(self,0x14)[psmInstanceB<unsigned int>(self,0x28)];
        psmInstanceB<unsigned char**>(self,0x14)[psmInstanceB<unsigned int>(self,0x28)]=0;
        psmRecordLink(record, 3)=manager->m_type3FreeList;
        manager->m_type3FreeList=record;
        ++psmInstanceB<unsigned int>(self,0x140);
        return 0;
    }

    void* descriptor=psmInstanceB<void*>(self,0x2C);
    // Wind only for a world-space system with a positive affection (ordered `>`); unlike type 0
    // the manager's wind-enabled byte (+0x8C10) is not consulted.
    float windX=0, windY=0, windZ=0;
    if (!psmDescriptor3<unsigned char>(descriptor,0x165) && psmDescriptor3<float>(descriptor,0xB8)>0)
    {
        const Vec3 wind=reinterpret_cast<world::WindManager*>(manager->m_windManager)->getWind(
            psmRuntimeField<Vec3>(record,0x44));
        windX=wind.x; windY=wind.y; windZ=wind.z;
    }
    descriptor=psmInstanceB<void*>(self,0x2C);
    const float affection=psmDescriptor3<float>(descriptor,0xB8);
    const float wx=static_cast<float>(static_cast<double>(affection)*windX);
    const float wy=static_cast<float>(static_cast<double>(windY)*affection);
    const float wz=static_cast<float>(static_cast<double>(windZ)*affection);
    const float gravityStep=static_cast<float>(static_cast<double>(step)*psmDescriptor3<float>(descriptor,0x98));

    // Air-resistance graph (+0xF4..+0x100) on the stored normalised age, then the transparency graph
    // (+0x114..+0x120). Both polynomials evaluate ((t*c2 + t^2*c1) + t^3*c0) + c3.
    double t=psmRuntimeField<float>(record,0x5C);
    const float dragCurve=static_cast<float>(t*psmDescriptor3<float>(descriptor,0xFC)
        +t*t*psmDescriptor3<float>(descriptor,0xF8)+t*t*t*psmDescriptor3<float>(descriptor,0xF4)
        +psmDescriptor3<float>(descriptor,0x100));
    descriptor=psmInstanceB<void*>(self,0x2C);
    const float drag=static_cast<float>(static_cast<double>(psmEmissionClamp(dragCurve))
        *psmDescriptor3<float>(descriptor,0x9C)*step);                   // sub_1000ED40(v, 0, 1)
    t=psmRuntimeField<float>(record,0x5C);
    const float alphaCurve=static_cast<float>(t*psmDescriptor3<float>(descriptor,0x11C)
        +t*t*psmDescriptor3<float>(descriptor,0x118)+t*t*t*psmDescriptor3<float>(descriptor,0x114)
        +psmDescriptor3<float>(descriptor,0x120));
    // [CHOSEN, not recovered] An unresolved child would fault here. Emit the qualified
    // diagnostic and stop at the same boundary instead of continuing without the child.
    if (!psmRecordPointer<IParticleSystemInstance*>(record, 3))
    {
        RD9_FRONTIER_AT("dice::hfe::geom::ParticleSystemEmitter::advanceParticle (child instance is null: "
                        "descriptor +0x88 never resolved; retail faults at 101819E3)", self, 0xA4);
        std::abort();
    }
    psmRecordPointer<IParticleSystemInstance*>(record, 3)->reserved22(psmEmissionClamp(alphaCurve));

    for (unsigned int remaining=steps; remaining; --remaining)
    {
        // The unrounded (step + age) feeds the normalised age, whose unrounded value feeds gravity.
        const double age=static_cast<double>(step)+psmRuntimeField<float>(record,0x7C);
        psmRuntimeField<float>(record,0x7C)=static_cast<float>(age);
        const double normalised=age*psmRuntimeField<float>(record,0x84);
        psmRuntimeField<float>(record,0x5C)=static_cast<float>(normalised);
        descriptor=psmInstanceB<void*>(self,0x2C);
        const float gravityCurve=static_cast<float>(normalised*psmDescriptor3<float>(descriptor,0xEC)
            +normalised*normalised*psmDescriptor3<float>(descriptor,0xE8)
            +normalised*normalised*normalised*psmDescriptor3<float>(descriptor,0xE4)
            +psmDescriptor3<float>(descriptor,0xF0));
        float accelY=static_cast<float>(static_cast<double>(psmEmissionClamp(gravityCurve))*gravityStep); // var_10
        // Quadratic drag against the wind-relative velocity, built from a normalised direction
        // (sub_100777A0) times -|v|^2 - not the reciprocal-sqrt form types 0/2/4 use.
        float relative[3]={
            static_cast<float>(static_cast<double>(psmRuntimeField<float>(record,0x70))-wx),
            static_cast<float>(static_cast<double>(psmRuntimeField<float>(record,0x74))-wy),
            static_cast<float>(static_cast<double>(psmRuntimeField<float>(record,0x78))-wz)};
        const float speedSquared=static_cast<float>(static_cast<double>(relative[0])*relative[0]
            +static_cast<double>(relative[2])*relative[2]+static_cast<double>(relative[1])*relative[1]);
        psmNormalize3(relative);
        const double negative=-static_cast<double>(speedSquared);
        const float forceX=static_cast<float>(relative[0]*negative);        // var_38
        const float forceY=static_cast<float>(relative[1]*negative);        // var_34
        const float forceZ=static_cast<float>(relative[2]*negative);        // var_30
        const float accelX=static_cast<float>(static_cast<double>(forceX)*drag);          // var_14
        const float dragY=static_cast<float>(static_cast<double>(forceY)*drag);           // var_4C
        const double accelZ=static_cast<double>(forceZ)*drag;                             // on stack
        accelY=static_cast<float>(static_cast<double>(dragY)+accelY);
        const float halfZ=static_cast<float>(accelZ*0.5);                                 // var_54
        const float midX=static_cast<float>(static_cast<double>(accelX)*0.5+psmRuntimeField<float>(record,0x70));
        const double midY=static_cast<double>(accelY)*0.5+psmRuntimeField<float>(record,0x74);
        const double midZ=static_cast<double>(halfZ)+psmRuntimeField<float>(record,0x78);
        const float moveX=static_cast<float>(static_cast<double>(midX)*step);             // var_44
        const float moveY=static_cast<float>(midY*step);                                  // var_40
        const double moveZ=midZ*step;
        psmRuntimeField<float>(record,0x44)=static_cast<float>(static_cast<double>(moveX)+psmRuntimeField<float>(record,0x44));
        psmRuntimeField<float>(record,0x48)=static_cast<float>(static_cast<double>(moveY)+psmRuntimeField<float>(record,0x48));
        psmRuntimeField<float>(record,0x4C)=static_cast<float>(moveZ+psmRuntimeField<float>(record,0x4C));
        psmRuntimeField<float>(record,0x70)=static_cast<float>(static_cast<double>(accelX)+psmRuntimeField<float>(record,0x70));
        psmRuntimeField<float>(record,0x74)=static_cast<float>(static_cast<double>(accelY)+psmRuntimeField<float>(record,0x74));
        psmRuntimeField<float>(record,0x78)=static_cast<float>(accelZ+psmRuntimeField<float>(record,0x78));
        // No near-zero velocity snap (types 0/4 have one).
    }

    descriptor=psmInstanceB<void*>(self,0x2C);
    if (psmDescriptor3<unsigned char>(descriptor,0xC4))
    {
        const double difference=static_cast<double>(psmInstanceB<float>(self,0x214))-psmRuntimeField<float>(record,0x7C);
        const int current=psmInstanceB<unsigned char>(self,0x210);
        int selected=current;
        // 10181B64 `test ah,1; jnz`: less-than OR unordered walks back through the history.
        if (!(difference>=0))
        {
            const int back=static_cast<int>(-3.0*difference);                  // RC=11 fistp
            selected=static_cast<int>(static_cast<unsigned int>(current)-static_cast<unsigned int>(back)-1u);
            if (selected<0)
                selected=static_cast<int>(static_cast<unsigned int>(selected)
                    +((0xFFFFFFFFu-static_cast<unsigned int>(selected))/10u)*10u+10u);
        }
        // No range guard: (selected + 0x16) << 4 is added to the instance as shipped.
        const float* const plane=selected==current ? &psmInstanceB<float>(self,0x200)
            : reinterpret_cast<const float*>(static_cast<unsigned char*>(self)
                +((static_cast<unsigned int>(selected)+0x16u)<<4));
        double velocityDot=static_cast<double>(psmRuntimeField<float>(record,0x78))*plane[2]
            +static_cast<double>(psmRuntimeField<float>(record,0x74))*plane[1]
            +static_cast<double>(psmRuntimeField<float>(record,0x70))*plane[0];
        if (velocityDot<0)
        {
            const double distance=static_cast<double>(psmRuntimeField<float>(record,0x4C))*plane[2]
                +static_cast<double>(psmRuntimeField<float>(record,0x48))*plane[1]
                +static_cast<double>(psmRuntimeField<float>(record,0x44))*plane[0]+plane[3];
            if (distance<0)
            {
                const float zDistance=static_cast<float>(distance*plane[2]);
                psmRuntimeField<float>(record,0x44)=static_cast<float>(psmRuntimeField<float>(record,0x44)-distance*plane[0]);
                psmRuntimeField<float>(record,0x48)=static_cast<float>(psmRuntimeField<float>(record,0x48)-distance*plane[1]);
                psmRuntimeField<float>(record,0x4C)-=zDistance;
                velocityDot=static_cast<double>(psmRuntimeField<float>(record,0x78))*plane[2]
                    +static_cast<double>(psmRuntimeField<float>(record,0x74))*plane[1]
                    +static_cast<double>(psmRuntimeField<float>(record,0x70))*plane[0];
                if (velocityDot<0)
                {
                    descriptor=psmInstanceB<void*>(self,0x2C);
                    const double nx=velocityDot*plane[0], ny=velocityDot*plane[1];
                    const float nz=static_cast<float>(velocityDot*plane[2]);
                    const float normal=psmDescriptor3<float>(descriptor,0xC8), along=psmDescriptor3<float>(descriptor,0xCC);
                    const float tx=static_cast<float>(psmRuntimeField<float>(record,0x70)-nx);
                    const float ty=static_cast<float>(psmRuntimeField<float>(record,0x74)-ny);
                    const float tz=psmRuntimeField<float>(record,0x78)-nz;
                    const float rx=static_cast<float>(nx*normal), ry=static_cast<float>(ny*normal);
                    psmRuntimeField<float>(record,0x78)=static_cast<float>(
                        static_cast<float>(tz*along)-static_cast<double>(nz)*normal);
                    psmRuntimeField<float>(record,0x70)=static_cast<float>(tx*along)-rx;
                    psmRuntimeField<float>(record,0x74)=static_cast<float>(ty*along)-ry;
                }
            }
        }
    }
    psmRuntimeField<unsigned int>(record,0x60)|=8u;
    float* const worldPosition=&psmRuntimeField<float>(record,0x50);
    psmRuntimeField<Vec3>(record,0x50)=psmRuntimeField<Vec3>(record,0x44);
    descriptor=psmInstanceB<void*>(self,0x2C);
    if (psmDescriptor3<unsigned char>(descriptor,0x165))
    {
        const float* const m=&psmInstanceB<float>(self,0x74);
        const double x=worldPosition[0], y=worldPosition[1], z=worldPosition[2];
        const double rz=static_cast<double>(m[10])*z+static_cast<double>(m[2])*x+static_cast<double>(m[6])*y+m[14];
        const double ry=static_cast<double>(m[9])*z+static_cast<double>(m[1])*x+static_cast<double>(m[5])*y+m[13];
        worldPosition[0]=static_cast<float>(static_cast<double>(m[8])*z+static_cast<double>(m[4])*y+static_cast<double>(m[0])*x+m[12]);
        worldPosition[1]=static_cast<float>(ry); worldPosition[2]=static_cast<float>(rz);
    }
    descriptor=psmInstanceB<void*>(self,0x2C);
    // Water placement without the descriptor's waterSurfaceOffset (+0xC0): types 2/4 add theirs.
    if (psmDescriptor3<unsigned char>(descriptor,0xBC) && manager->m_heightmapCluster)
        worldPosition[1]=reinterpret_cast<world::IHeightmapCluster*>(manager->m_heightmapCluster)
            ->getWaterLevelInWorldCoords(*reinterpret_cast<const Vec3*>(worldPosition));
    // PE 1017A2B0 (by-value x, y, z): ordered extrema, so NaN never moves a bound.
    {
        const float x=worldPosition[0], y=worldPosition[1], z=worldPosition[2];
        float* const bounds=&psmInstanceB<float>(self,0xFC);
        if (x>bounds[3]) bounds[3]=x;
        if (x<bounds[0]) bounds[0]=x;
        if (y>bounds[4]) bounds[4]=y;
        if (y<bounds[1]) bounds[1]=y;
        if (z>bounds[5]) bounds[5]=z;
        if (z<bounds[2]) bounds[2]=z;
    }
    // Rotation graph (+0xD4..+0xE0), evaluated ((t^2*c1 + t*c2) + t^3*c0) + c3, scaled by the whole
    // advance (float(steps) * step).
    descriptor=psmInstanceB<void*>(self,0x2C);
    t=psmRuntimeField<float>(record,0x5C);
    const float rotationCurve=static_cast<float>(t*t*psmDescriptor3<float>(descriptor,0xD8)
        +t*psmDescriptor3<float>(descriptor,0xDC)+t*t*t*psmDescriptor3<float>(descriptor,0xD4)
        +psmDescriptor3<float>(descriptor,0xE0));
    const double rotation=static_cast<double>(psmEmissionClamp(rotationCurve))*stepsF*step;
    float* const matrix=&psmRuntimeField<float>(record,0x04);
    descriptor=psmInstanceB<void*>(self,0x2C);
    if (psmDescriptor3<unsigned char>(descriptor,0x164))
    {
        // alignRotationToSpeed: the particle frame looks along its velocity.
        const float vx=psmRuntimeField<float>(record,0x70), vy=psmRuntimeField<float>(record,0x74);
        const float vz=psmRuntimeField<float>(record,0x78);
        const double length=::sqrt(static_cast<double>(vx)*vx+static_cast<double>(vy)*vy+static_cast<double>(vz)*vz);
        if (::fabs(length)<1.1920929e-7f)
        {
            // sixteen inline identity stores, then the translation words
            // +0x34/+0x38/+0x3C. The rotation amount computed above is simply discarded.
            *reinterpret_cast<Mat4*>(matrix)=Mat4::getIdentity();
            matrix[12]=worldPosition[0]; matrix[13]=worldPosition[1]; matrix[14]=worldPosition[2];
        }
        else
        {
            const double inverse=1.0/length;
            float forward[3]={static_cast<float>(inverse*vx), static_cast<float>(static_cast<double>(vy)*inverse),
                              static_cast<float>(static_cast<double>(vz)*inverse)};
            float right[3], up[3];
            if (::fabs(forward[0])<9.9999997e-05f && ::fabs(forward[2])<9.9999997e-05f)
            {
                // Vertical travel: a fixed frame. fx/fz are overwritten with 0, and a zero or NaN
                // fy selects the downward frame (10181EF8 `test ah,41h; jnz`).
                right[0]=1; right[1]=0; right[2]=0; up[0]=0; up[1]=0;
                forward[0]=0; forward[2]=0;
                if (forward[1]>0) { forward[1]=1; up[2]=-1; }
                else { forward[1]=-1; up[2]=1; }
            }
            else
            {
                // right = (0,1,0) x forward, with the literal zero products kept (they matter for inf/NaN).
                right[0]=static_cast<float>(forward[2]-static_cast<double>(forward[1])*0.0);
                right[1]=static_cast<float>(static_cast<double>(forward[0])*0.0-static_cast<double>(forward[2])*0.0);
                right[2]=static_cast<float>(static_cast<double>(forward[1])*0.0-forward[0]);
                psmNormalize3(right);
                up[0]=static_cast<float>(static_cast<double>(right[2])*forward[1]-static_cast<double>(right[1])*forward[2]);
                up[1]=static_cast<float>(static_cast<double>(forward[2])*right[0]-static_cast<double>(right[2])*forward[0]);
                up[2]=static_cast<float>(static_cast<double>(right[1])*forward[0]-static_cast<double>(forward[1])*right[0]);
            }
            psmType3SetBasis(matrix, right, up, forward, worldPosition);
        }
    }
    else
    {
        const double spinX=rotation*psmRuntimeField<float>(record,0x88), spinY=rotation*psmRuntimeField<float>(record,0x8C);
        const float spinZ=static_cast<float>(rotation*psmRuntimeField<float>(record,0x90));
        psmRuntimeField<float>(record,0x94)=static_cast<float>(spinX+psmRuntimeField<float>(record,0x94));
        psmRuntimeField<float>(record,0x98)=static_cast<float>(spinY+psmRuntimeField<float>(record,0x98));
        psmRuntimeField<float>(record,0x9C)=static_cast<float>(static_cast<double>(spinZ)+psmRuntimeField<float>(record,0x9C));
        // PE 10078730 is the RendDX9 copy of the tree's setRotationZXY (same z/x/y bit set, same
        // seven-way product, nine rotation elements only). Arguments: -rot.z, -rot.y, -rot.x.
        setRotationZXY(*reinterpret_cast<Mat4*>(matrix), -psmRuntimeField<float>(record,0x9C),
                       -psmRuntimeField<float>(record,0x98), -psmRuntimeField<float>(record,0x94));
        matrix[12]=worldPosition[0]; matrix[13]=worldPosition[1]; matrix[14]=worldPosition[2];
    }

    // speedFromParticle (+0x168): the child inherits the particle's velocity as speed + direction.
    // (Its console binding is missing on the world side today - see readme - so it stays 0.)
    descriptor=psmInstanceB<void*>(self,0x2C);
    if (psmDescriptor3<float>(descriptor,0x168)>0)
    {
        const float factor=psmDescriptor3<float>(descriptor,0x168);
        IParticleSystemInstance* const child=psmRecordPointer<IParticleSystemInstance*>(record, 3);
        float direction[3];
        direction[0]=static_cast<float>(static_cast<double>(factor)*psmRuntimeField<float>(record,0x70));
        direction[1]=static_cast<float>(static_cast<double>(factor)*psmRuntimeField<float>(record,0x74));
        const double zProduct=static_cast<double>(factor)*psmRuntimeField<float>(record,0x78);
        direction[2]=static_cast<float>(zProduct);
        // the z term multiplies the unrounded product by its own rounded copy.
        const double length=::sqrt(zProduct*direction[2]+static_cast<double>(direction[1])*direction[1]
            +static_cast<double>(direction[0])*direction[0]);
        const float speed=static_cast<float>(length);
        if (::fabs(length)<1.1920929e-7f)
            child->reserved19(0.0f);                                   // direction left as it was
        else
        {
            const double inverse=1.0/speed;                            // divides by the float speed
            direction[0]=static_cast<float>(direction[0]*inverse);
            direction[1]=static_cast<float>(direction[1]*inverse);
            direction[2]=static_cast<float>(direction[2]*inverse);
            child->reserved19(speed);
            psmRecordPointer<IParticleSystemInstance*>(record, 3)->reserved16(*reinterpret_cast<const Vec3*>(direction));
        }
    }
    // Unguarded in retail (101820F1) like the transparency call; a null child already stopped above.
    // The child advances by this particle's whole span (float(steps * step)), in its own time scale.
    psmRecordPointer<IParticleSystemInstance*>(record, 3)->reserved41(span);
    return 1;
}

// PE 10183640, 907 B: the frame update (virtual +0xA4).
void psmType3Update(void* self, float dt)
{
    IParticleSystemInstance* const instance=static_cast<IParticleSystemInstance*>(self);
    float elapsed=static_cast<float>(static_cast<double>(dt)*psmInstanceB<float>(self,0x128));
    if (!psmInstanceB<unsigned int>(self,0x28) && !psmInstanceB<unsigned char>(self,0x144))
    {
        psmQueueInactive(reinterpret_cast<ParticleSystemManager*>(g_ParticleSystemManager),instance);
        if (psmInstanceB<unsigned char>(self,0x18))
            reinterpret_cast<IParticleSystemManager*>(g_ParticleSystemManager)->removeInstance(instance);
        return;
    }
    // The source dt refuses (NaN passes, as `test ah,41h; jnp` only rejects an ordered <= 0).
    if (psmInstanceB<unsigned char>(self,0x21D) || dt<=0) return;
    void* descriptor=psmInstanceB<void*>(self,0x2C);
    if (psmDescriptor3<unsigned char>(descriptor,0xC4))
    {
        psmInstanceB<float>(self,0x214)=static_cast<float>(static_cast<double>(elapsed)+psmInstanceB<float>(self,0x214));
        psmType3RememberPlane(self);
    }
    if (!psmInstanceB<unsigned char>(self,0x21F) && psmInstanceB<unsigned char>(self,0x21E))
        psmInstanceB<unsigned char>(self,0x21F)=psmInstanceB<unsigned char>(self,0x21E);
    descriptor=psmInstanceB<void*>(self,0x2C);
    psmInstanceB<unsigned int>(self,0x140)=0; psmInstanceB<unsigned int>(self,0x13C)=0;
    const double maximum=static_cast<double>(psmDescriptor3<float>(descriptor,0x90))+psmDescriptor3<float>(descriptor,0x8C);
    if (elapsed>maximum) elapsed=static_cast<float>(maximum);
    for (int i=0;i<3;++i)
    { psmInstanceB<unsigned int>(self,0xFC+4*i)=0x4B18967Fu; psmInstanceB<unsigned int>(self,0x108+4*i)=0xCB18967Fu; }
    psmInstanceB<Vec3>(self,0xC0)=psmInstanceB<Vec3>(self,0xA4);
    psmType3RefreshTransform(self);
    psmInstanceB<Vec3>(self,0xB4)=psmInstanceB<Vec3>(self,0xA4);
    const double total=static_cast<double>(elapsed)+psmInstanceB<float>(self,0xD0);
    const unsigned int steps=static_cast<unsigned int>(static_cast<int>(100.0*total));
    const double advanced=static_cast<double>(steps)*psmType3TimeStep;
    psmInstanceB<float>(self,0xCC)=psmInstanceB<float>(self,0xF4);
    psmInstanceB<float>(self,0xD0)=static_cast<float>(total-advanced);
    psmInstanceB<float>(self,0xF4)=static_cast<float>(advanced+psmInstanceB<float>(self,0xF4));
    for (unsigned int i=0;psmInstanceB<unsigned char**>(self,0x14)[i];)
        i+=psmType3AdvanceParticle(self,psmInstanceB<unsigned char**>(self,0x14)[i],static_cast<int>(i),
                                   psmType3TimeStep,steps);
    const double delay=static_cast<double>(psmInstanceB<float>(self,0xF0))-elapsed;
    psmInstanceB<float>(self,0xF0)=static_cast<float>(delay);
    if (delay>0) { psmInstanceB<float>(self,0xF8)=psmInstanceB<float>(self,0xF4); return; }
    // Prewarm (+0xEC, from the descriptor at start) runs here as recursive virtual updates of at most
    // 1/60 s. The loop test uses the unrounded remainder (`fst` then `fcomp`).
    if (psmInstanceB<float>(self,0xEC)>0)
    {
        float remaining=psmInstanceB<float>(self,0xEC);
        psmInstanceB<float>(self,0xEC)=0;
        if (remaining>0)
            for (;;)
            {
                instance->reserved41(remaining<0.016666668f ? remaining : 0.016666668f);
                const double next=static_cast<double>(remaining)-0.016666668f;
                remaining=static_cast<float>(next);
                if (!(next>0)) break;
            }
    }
    psmType3Emit(self,elapsed);
    psmInstanceB<Vec3>(self,0xA4)=psmInstanceB<Vec3>(self,0xB4);
    float* const bounds=&psmInstanceB<float>(self,0xFC);
    if (psmInstanceB<unsigned int>(self,0x28))
    {
        // A constant 1.0 margin (types 0/2/4 pad by a descriptor size instead).
        for (int i=0;i<3;++i) bounds[i]=static_cast<float>(static_cast<double>(bounds[i])-1.0);
        for (int i=3;i<6;++i) bounds[i]=static_cast<float>(static_cast<double>(bounds[i])+1.0);
    }
    else
    {
        psmInstanceB<Vec3>(self,0xFC)=psmInstanceB<Vec3>(self,0xA4);
        psmInstanceB<Vec3>(self,0x108)=psmInstanceB<Vec3>(self,0xA4);
    }
    // PE 1016EB70 / 1016EBC0 (the same aabb radius/centre helpers the type-0 update inlines).
    const float dz=bounds[5]-bounds[2], dx=static_cast<float>((static_cast<double>(bounds[3])-bounds[0])*0.5);
    const double dy=(static_cast<double>(bounds[4])-bounds[1])*0.5;
    psmInstanceB<float>(self,0x120)=static_cast<float>(::sqrt(dz*0.5*(dz*0.5)+static_cast<double>(dx)*dx+dy*dy));
    const float zsum=bounds[5]+bounds[2];
    psmInstanceB<Vec3>(self,0x114)={static_cast<float>((static_cast<double>(bounds[3])+bounds[0])*0.5),
        static_cast<float>((static_cast<double>(bounds[4])+bounds[1])*0.5),zsum*0.5f};
}
