#include "stdafx.h"
#include "Portal.h"
#include "GameObjectTypes.h"

BoundingOrientedBox Portal::GetOBB()
{
	BoundingOrientedBox obb_local = Shape::IndexToShapeMap[ShapeIndex].GetMesh()->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, m_worldMatrix);
	return obb_world;
}
