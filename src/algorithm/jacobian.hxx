//
// Copyright (c) 2015-2019 CNRS INRIA
//

#ifndef __pinocchio_jacobian_hxx__
#define __pinocchio_jacobian_hxx__

#include "pinocchio/multibody/visitor.hpp"
#include "pinocchio/algorithm/check.hpp"

/// @cond DEV

namespace pinocchio
{
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType>
  struct JointJacobiansForwardStep
  : public fusion::JointVisitorBase< JointJacobiansForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &,
                                  const ConfigVectorType &> ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      
      jmodel.calc(jdata.derived(),q.derived());
      
      data.liMi[i] = model.jointPlacements[i]*jdata.M();
      if(parent>0) data.oMi[i] = data.oMi[parent]*data.liMi[i];
      else         data.oMi[i] = data.liMi[i];
      
      jmodel.jointCols(data.J) = data.oMi[i].act(jdata.S());
    }
  
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::Matrix6x &
  computeJointJacobians(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                        DataTpl<Scalar,Options,JointCollectionTpl> & data,
                        const Eigen::MatrixBase<ConfigVectorType> & q)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    typedef JointJacobiansForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType> Pass;
    for(JointIndex i=1; i<(JointIndex) model.njoints; ++i)
    {
      Pass::run(model.joints[i],data.joints[i],
                typename Pass::ArgsType(model,data,q.derived()));
    }
  
    return data.J;
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl>
  struct JointJacobiansForwardStep2
  : public fusion::JointVisitorBase< JointJacobiansForwardStep2<Scalar,Options,JointCollectionTpl> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<Data &> ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     Data & data)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      jmodel.jointCols(data.J) = data.oMi[i].act(jdata.S());
    }
    
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::Matrix6x &
  computeJointJacobians(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                        DataTpl<Scalar,Options,JointCollectionTpl> & data)
  {
    assert(model.check(data) && "data is not consistent with model.");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    typedef JointJacobiansForwardStep2<Scalar,Options,JointCollectionTpl> Pass;
    for(JointIndex i=1; i< (JointIndex)model.njoints; ++i)
    {
      Pass::run(model.joints[i],data.joints[i],
                typename Pass::ArgsType(data));
    }
    
    return data.J;
  }
  
  /* Return the jacobian of the output frame attached to joint <jointId> in the
   world frame or in the local frame depending on the template argument. The
   function computeJacobians should have been called first. */
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename Matrix6xLike>
  inline void getJointJacobian(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                               const DataTpl<Scalar,Options,JointCollectionTpl> & data,
                               const typename ModelTpl<Scalar,Options,JointCollectionTpl>::JointIndex jointId,
                               const ReferenceFrame rf,
                               const Eigen::MatrixBase<Matrix6xLike> & J)
  {
    assert( J.rows() == 6 );
    assert( J.cols() == model.nv );
    assert(model.check(data) && "data is not consistent with model.");
    
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    Matrix6xLike & J_ = PINOCCHIO_EIGEN_CONST_CAST(Matrix6xLike,J);
    
    const typename Data::SE3 & oMjoint = data.oMi[jointId];
    int colRef = nv(model.joints[jointId])+idx_v(model.joints[jointId])-1;
    for(Eigen::DenseIndex j=colRef;j>=0;j=data.parents_fromRow[(Model::Index)j])
    {
      typedef typename Data::Matrix6x::ConstColXpr ConstColXprIn;
      const MotionRef<ConstColXprIn> v_in(data.J.col(j));
      
      typedef typename Matrix6xLike::ColXpr ColXprOut;
      MotionRef<ColXprOut> v_out(J_.col(j));
      
      switch(rf)
      {
        case WORLD:
          v_out = v_in;
          break;
        case LOCAL_WORLD_ALIGNED:
          v_out = v_in;
          v_out.linear() -= oMjoint.translation().cross(v_in.angular());
          break;
        case LOCAL:
          v_out = oMjoint.actInv(v_in);
          break;
        default:
          assert(false && "must never happened");
          break;
      }
    }
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename Matrix6xLike>
  struct JointJacobianForwardStep
  : public fusion::JointVisitorBase< JointJacobianForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,Matrix6xLike> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &,
                                  const ConfigVectorType &,
                                  Matrix6xLike &
                                  > ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q,
                     const Eigen::MatrixBase<Matrix6xLike> & J)
    {
      typedef typename Model::JointIndex JointIndex;
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      
      jmodel.calc(jdata.derived(),q.derived());
      
      data.liMi[i] = model.jointPlacements[i]*jdata.M();
      data.iMf[parent] = data.liMi[i]*data.iMf[i];
      
      Matrix6xLike & J_ = PINOCCHIO_EIGEN_CONST_CAST(Matrix6xLike,J);
      jmodel.jointCols(J_) = data.iMf[i].actInv(jdata.S());
    }
  
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename Matrix6xLike>
  inline void jointJacobian(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                            DataTpl<Scalar,Options,JointCollectionTpl> & data,
                            const Eigen::MatrixBase<ConfigVectorType> & q,
                            const JointIndex jointId,
                            const Eigen::MatrixBase<Matrix6xLike> & J)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    data.iMf[jointId].setIdentity();
    typedef JointJacobianForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,Matrix6xLike> Pass;
    for(JointIndex i=jointId; i>0; i=model.parents[i])
    {
      Pass::run(model.joints[i],data.joints[i],
                typename Pass::ArgsType(model,data,q.derived(),PINOCCHIO_EIGEN_CONST_CAST(Matrix6xLike,J)));
    }
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType>
  struct JointJacobiansTimeVariationForwardStep
  : public fusion::JointVisitorBase< JointJacobiansTimeVariationForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &,
                                  const ConfigVectorType &,
                                  const TangentVectorType &> ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q,
                     const Eigen::MatrixBase<TangentVectorType> & v)
    {
      typedef typename Model::JointIndex JointIndex;
      typedef typename Data::SE3 SE3;
      typedef typename Data::Motion Motion;
      
      const JointIndex & i = (JointIndex) jmodel.id();
      const JointIndex & parent = model.parents[i];
      
      SE3 & oMi = data.oMi[i];
      Motion & vJ = data.v[i];
      
      jmodel.calc(jdata.derived(),q.derived(),v.derived());
      
      vJ = jdata.v();
      
      data.liMi[i] = model.jointPlacements[i]*jdata.M();
      if(parent>0)
      {
        oMi = data.oMi[parent]*data.liMi[i];
        vJ += data.liMi[i].actInv(data.v[parent]);
      }
      else
      {
        oMi = data.liMi[i];
      }
      
      jmodel.jointCols(data.J) = oMi.act(jdata.S());
      
      // Spatial velocity of joint i expressed in the global frame o
      data.ov[i] = oMi.act(vJ);
      
      typedef typename SizeDepType<JointModel::NV>::template ColsReturn<typename Data::Matrix6x>::Type ColsBlock;
      ColsBlock dJcols = jmodel.jointCols(data.dJ);
      ColsBlock Jcols = jmodel.jointCols(data.J);
      
      motionSet::motionAction(data.ov[i],Jcols,dJcols);
    }
    
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::Matrix6x &
  computeJointJacobiansTimeVariation(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                                     DataTpl<Scalar,Options,JointCollectionTpl> & data,
                                     const Eigen::MatrixBase<ConfigVectorType> & q,
                                     const Eigen::MatrixBase<TangentVectorType> & v)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    assert(v.size() == model.nv && "The velocity vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    typedef JointJacobiansTimeVariationForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType> Pass;
    for(JointIndex i=1; i<(JointIndex)model.njoints; ++i)
    {
      Pass::run(model.joints[i],data.joints[i],
                typename Pass::ArgsType(model,data,q.derived(),v.derived()));
    }
    
    return data.dJ;
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename Matrix6xLike>
  inline void getJointJacobianTimeVariation(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                                            const DataTpl<Scalar,Options,JointCollectionTpl> & data,
                                            const JointIndex jointId,
                                            const ReferenceFrame rf,
                                            const Eigen::MatrixBase<Matrix6xLike> & dJ)
  {
    assert( dJ.rows() == 6 );
    assert( dJ.cols() == model.nv );
    assert(model.check(data) && "data is not consistent with model.");
    
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    Matrix6xLike & dJ_ = PINOCCHIO_EIGEN_CONST_CAST(Matrix6xLike,dJ);
    
    const typename Data::SE3 & oMjoint = data.oMi[jointId];
    int colRef = nv(model.joints[jointId])+idx_v(model.joints[jointId])-1;
    for(Eigen::DenseIndex j=colRef;j>=0;j=data.parents_fromRow[(size_t)j])
    {
      typedef typename Data::Matrix6x::ConstColXpr ConstColXprIn;
      const MotionRef<ConstColXprIn> v_in(data.dJ.col(j));
      
      typedef typename Matrix6xLike::ColXpr ColXprOut;
      MotionRef<ColXprOut> v_out(dJ_.col(j));
      
      if(rf == WORLD)   v_out = v_in;
      else              v_out = oMjoint.actInv(v_in).toVector();
    }
  }
  
  
} // namespace pinocchio

/// @endcond

#endif // ifndef __pinocchio_jacobian_hxx__
