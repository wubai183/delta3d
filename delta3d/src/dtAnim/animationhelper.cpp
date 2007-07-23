/* -*-c++-*-
 * Delta3D Open Source Game and Simulation Engine
 * Copyright (C) 2007, Alion Science and Technology
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Bradley Anderegg 03/30/2007
 */

#include <dtAnim/animationhelper.h>
#include <dtAnim/animnodebuilder.h>
#include <dtAnim/cal3ddatabase.h>
#include <dtAnim/cal3dmodeldata.h>
#include <dtAnim/cal3dmodelwrapper.h>
#include <dtAnim/cal3danimator.h>
#include <dtAnim/ical3ddriver.h>
#include <dtAnim/sequencemixer.h>
#include <dtAnim/animationsequence.h>
#include <dtAnim/animationchannel.h>
#include <dtAnim/animationwrapper.h>
#include <dtAnim/attachmentcontroller.h>

#include <dtDAL/actorproperty.h>
#include <dtDAL/enginepropertytypes.h>
#include <dtDAL/actorproxy.h>

#include <dtCore/hotspotattachment.h>

#include <dtUtil/log.h>

#include <osg/Geode>
#include <osg/Texture2D>


namespace dtAnim
{

dtCore::RefPtr<Cal3DDatabase> AnimationHelper::sModelDatabase = new Cal3DDatabase();

/////////////////////////////////////////////////////////////////////////////////
AnimationHelper::AnimationHelper()
: mGroundClamp(false)
, mGeode(NULL)
, mAnimator(NULL)
, mNodeBuilder(new AnimNodeBuilder())
, mSequenceMixer(new SequenceMixer())
, mAttachmentController(new AttachmentController())
{
}

/////////////////////////////////////////////////////////////////////////////////
AnimationHelper::~AnimationHelper()
{
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationHelper::Update(float dt)
{
   if(mAnimator.valid())
   {
      mSequenceMixer->Update(dt);
      mAnimator->Update(dt);
      mAttachmentController->Update(*GetModelWrapper());
   }
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationHelper::PlayAnimation(const std::string& pAnim)
{
   const Animatable* anim = mSequenceMixer->GetRegisteredAnimation(pAnim);

   if(anim != NULL)
   {
      dtCore::RefPtr<Animatable> clonedAnim = anim->Clone(mAnimator->GetWrapper());      
      mSequenceMixer->PlayAnimation(clonedAnim.get());
   }
   else
   {
      LOG_ERROR("Cannot play animation '" + pAnim + 
            "' because it has not been registered with the SequenceMixer.")
   }
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationHelper::ClearAnimation(const std::string& pAnim, float fadeOutTime)
{
   mSequenceMixer->ClearAnimation(pAnim, fadeOutTime);
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationHelper::ClearAll(float fadeOut)
{
   mSequenceMixer->ClearActiveAnimations(fadeOut);
}

/////////////////////////////////////////////////////////////////////////////////
bool AnimationHelper::LoadModel(const std::string& pFilename)
{
   dtCore::RefPtr<dtAnim::Cal3DModelWrapper> newModel = sModelDatabase->Load(pFilename);

   if (newModel.valid())
   {
      mAnimator = new dtAnim::Cal3DAnimator(newModel.get());   
      mGeode = mNodeBuilder->CreateGeode(newModel.get());
      
      const Cal3DModelData*  modelData = sModelDatabase->GetModelData(*newModel);
      if(modelData == NULL)
      {
         LOG_ERROR("ModelData not found for Character XML '" + pFilename + "'");
         return false;
      }

      const Cal3DModelData::AnimatableArray& animatables = modelData->GetAnimatables();

      Cal3DModelData::AnimatableArray::const_iterator iter = animatables.begin();
      Cal3DModelData::AnimatableArray::const_iterator end = animatables.end();

      for(;iter != end; ++iter)
      {
         mSequenceMixer->RegisterAnimation((*iter).get());
      }

   }
   else
   {
      LOG_ERROR("Unable to load skeletal resource: " + pFilename);
      return false;
   }
   return true;
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationHelper::GetActorProperties(dtDAL::ActorProxy& pProxy, 
      std::vector<dtCore::RefPtr<dtDAL::ActorProperty> >& pFillVector)
{
   pFillVector.push_back(new dtDAL::ResourceActorProperty(pProxy, dtDAL::DataType::SKELETAL_MESH,
      "Skeletal Mesh", "Skeletal Mesh", dtDAL::MakeFunctor(*this, &AnimationHelper::LoadModel),
      "The model resource that defines the skeletal mesh", "AnimationModel"));     
}

/////////////////////////////////////////////////////////////////////////////////
osg::Geode* AnimationHelper::GetGeode()
{
   return mGeode.get();
}

/////////////////////////////////////////////////////////////////////////////////
const osg::Geode* AnimationHelper::GetGeode() const
{
   return mGeode.get();
}

/////////////////////////////////////////////////////////////////////////////////
Cal3DAnimator* AnimationHelper::GetAnimator()
{
   return mAnimator.get();
}

/////////////////////////////////////////////////////////////////////////////////
const Cal3DModelWrapper* AnimationHelper::GetModelWrapper() const
{
   return mAnimator->GetWrapper();
}

/////////////////////////////////////////////////////////////////////////////////
Cal3DModelWrapper* AnimationHelper::GetModelWrapper()
{
   return mAnimator->GetWrapper();
}

/////////////////////////////////////////////////////////////////////////////////
const Cal3DAnimator* AnimationHelper::GetAnimator() const
{
   return mAnimator.get();
}

/////////////////////////////////////////////////////////////////////////////////
SequenceMixer& AnimationHelper::GetSequenceMixer()
{
   return *mSequenceMixer;
}

/////////////////////////////////////////////////////////////////////////////////
const SequenceMixer& AnimationHelper::GetSequenceMixer() const
{
   return *mSequenceMixer;
}

/////////////////////////////////////////////////////////////////////////////////
bool AnimationHelper::GetGroundClamp() const
{
   return mGroundClamp;
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationHelper::SetGroundClamp(bool b)
{
   mGroundClamp = b;
}

/////////////////////////////////////////////////////////////////////////////////
AttachmentController& AnimationHelper::GetAttachmentController()
{
   return *mAttachmentController;
}

/////////////////////////////////////////////////////////////////////////////////
void AnimationHelper::SetAttachmentController(AttachmentController& newController)
{
   mAttachmentController = &newController;
}


}//namespace dtAnim
