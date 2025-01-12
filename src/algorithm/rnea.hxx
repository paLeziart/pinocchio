//
// Copyright (c) 2015-2018 CNRS
//

#ifndef __pinocchio_rnea_hxx__
#define __pinocchio_rnea_hxx__

/// @cond DEV

#include "pinocchio/multibody/visitor.hpp"
#include "pinocchio/algorithm/check.hpp"

namespace pinocchio
{
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType1, typename TangentVectorType2>
  struct RneaForwardStep
  : public fusion::JointVisitorBase< RneaForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType1,TangentVectorType2> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &,
                                  const ConfigVectorType &,
                                  const TangentVectorType1 &,
                                  const TangentVectorType2 &
                                  > ArgsType;

    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q,
                     const Eigen::MatrixBase<TangentVectorType1> & v,
                     const Eigen::MatrixBase<TangentVectorType2> & a)
    {
      typedef typename Model::JointIndex JointIndex;

      const JointIndex i = jmodel.id();
      const JointIndex parent = model.parents[i];

      jmodel.calc(jdata.derived(),q.derived(),v.derived());

      data.liMi[i] = model.jointPlacements[i]*jdata.M();
//      data.liMi[i].setIdentity();

      data.v[i] = jdata.v();
      if(parent>0)
        data.v[i] += data.liMi[i].actInv(data.v[parent]);
      
      data.a_gf[i] = jdata.c() + (data.v[i] ^ jdata.v());
      data.a_gf[i] += jdata.S() * jmodel.jointVelocitySelector(a);
      data.a_gf[i] += data.liMi[i].actInv(data.a_gf[parent]);
//
//      data.f[i] = model.inertias[i]*data.a_gf[i];// + model.inertias[i].vxiv(data.v[i]); // -f_ext
//      data.h[i] = model.inertias[i]*data.v[i];
      model.inertias[i].__mult__(data.v[i],data.h[i]);
      model.inertias[i].__mult__(data.a_gf[i],data.f[i]);
      data.f[i] += data.v[i].cross(data.h[i]);
//      data.h[i].motionAction(data.v[i],data.f[i]);
//      data.f[i] = model.inertias[i].vxiv(data.v[i]);
//      data.f[i].setZero();
    }

  };

  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl>
  struct RneaBackwardStep
  : public fusion::JointVisitorBase< RneaBackwardStep<Scalar,Options,JointCollectionTpl> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &
                                  > ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex i = jmodel.id();
      const JointIndex parent = model.parents[i];
      
      jmodel.jointVelocitySelector(data.tau) = jdata.S().transpose()*data.f[i];
      if(parent>0) data.f[parent] += data.liMi[i].act(data.f[i]);
    }
  };

  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType1, typename TangentVectorType2>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::TangentVectorType &
  rnea(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
       DataTpl<Scalar,Options,JointCollectionTpl> & data,
       const Eigen::MatrixBase<ConfigVectorType> & q,
       const Eigen::MatrixBase<TangentVectorType1> & v,
       const Eigen::MatrixBase<TangentVectorType2> & a)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    assert(v.size() == model.nv && "The velocity vector is not of right size");
    assert(a.size() == model.nv && "The acceleration vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    data.v[0].setZero();
    data.a_gf[0] = -model.gravity;

    typedef RneaForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType1,TangentVectorType2> Pass1;
    typename Pass1::ArgsType arg1(model,data,q.derived(),v.derived(),a.derived());
    for(JointIndex i=1; i<(JointIndex)model.njoints; ++i)
    {
      Pass1::run(model.joints[i],data.joints[i],
                 arg1);
    }
    
    typedef RneaBackwardStep<Scalar,Options,JointCollectionTpl> Pass2;
    typename Pass2::ArgsType arg2(model,data);
    for(JointIndex i=(JointIndex)model.njoints-1; i>0; --i)
    {
      Pass2::run(model.joints[i],data.joints[i],
                 arg2);
    }

    return data.tau;
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType1, typename TangentVectorType2, typename ForceDerived>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::TangentVectorType &
  rnea(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
       DataTpl<Scalar,Options,JointCollectionTpl> & data,
       const Eigen::MatrixBase<ConfigVectorType> & q,
       const Eigen::MatrixBase<TangentVectorType1> & v,
       const Eigen::MatrixBase<TangentVectorType2> & a,
       const container::aligned_vector<ForceDerived> & fext)
  {
    assert(fext.size() == model.joints.size());
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    assert(v.size() == model.nv && "The velocity vector is not of right size");
    assert(a.size() == model.nv && "The acceleration vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    data.v[0].setZero();
    data.a_gf[0] = -model.gravity;
    
    typedef RneaForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType1,TangentVectorType2> Pass1;
    for(JointIndex i=1; i<(JointIndex)model.njoints; ++i)
    {
      Pass1::run(model.joints[i],data.joints[i],
                 typename Pass1::ArgsType(model,data,q.derived(),v.derived(),a.derived()));
      data.f[i] -= fext[i];
    }
    
    typedef RneaBackwardStep<Scalar,Options,JointCollectionTpl> Pass2;
    for(JointIndex i=(JointIndex)model.njoints-1; i>0; --i)
    {
      Pass2::run(model.joints[i],data.joints[i],
                 typename Pass2::ArgsType(model,data));
    }

    
    return data.tau;
  }
 
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType>
  struct NLEForwardStep
  : public fusion::JointVisitorBase< NLEForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &,
                                  const ConfigVectorType &,
                                  const TangentVectorType &
                                  > ArgsType;
    
    template<typename JointModel>
    static void algo(const pinocchio::JointModelBase<JointModel> & jmodel,
                     pinocchio::JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q,
                     const Eigen::MatrixBase<TangentVectorType> & v)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      
      jmodel.calc(jdata.derived(),q.derived(),v.derived());
      
      data.liMi[i] = model.jointPlacements[i]*jdata.M();
      
      data.v[i] = jdata.v();
      if(parent>0) data.v[i] += data.liMi[i].actInv(data.v[parent]);
      
      data.a_gf[i]  = jdata.c() + (data.v[i] ^ jdata.v());
      data.a_gf[i] += data.liMi[i].actInv(data.a_gf[parent]);
      
      data.f[i] = model.inertias[i]*data.a_gf[i] + model.inertias[i].vxiv(data.v[i]); // -f_ext
    }
    
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl>
  struct NLEBackwardStep
  : public fusion::JointVisitorBase< NLEBackwardStep<Scalar,Options,JointCollectionTpl> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &
                                  >  ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent  = model.parents[i];
      
      jmodel.jointVelocitySelector(data.nle) = jdata.S().transpose()*data.f[i];
      if(parent>0) data.f[parent] += data.liMi[i].act(data.f[i]);
    }
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::TangentVectorType &
  nonLinearEffects(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                   DataTpl<Scalar,Options,JointCollectionTpl> & data,
                   const Eigen::MatrixBase<ConfigVectorType> & q,
                   const Eigen::MatrixBase<TangentVectorType> & v)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    assert(v.size() == model.nv && "The velocity vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    data.v[0].setZero ();
    data.a_gf[0] = -model.gravity;
    
    typedef NLEForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType> Pass1;
    for(JointIndex i=1; i<(JointIndex)model.njoints; ++i)
    {
      Pass1::run(model.joints[i],data.joints[i],
                 typename Pass1::ArgsType(model,data,q.derived(),v.derived()));
    }
    
    typedef NLEBackwardStep<Scalar,Options,JointCollectionTpl> Pass2;
    for(JointIndex i=(JointIndex)(model.njoints-1); i>0; --i)
    {
      Pass2::run(model.joints[i],data.joints[i],
                 typename Pass2::ArgsType(model,data));
    }
    
    return data.nle;
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType>
  struct ComputeGeneralizedGravityForwardStep
  : public fusion::JointVisitorBase< ComputeGeneralizedGravityForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &,
                                  const ConfigVectorType &
                                  > ArgsType;
    
    template<typename JointModel>
    static void algo(const pinocchio::JointModelBase<JointModel> & jmodel,
                     pinocchio::JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      
      jmodel.calc(jdata.derived(),q.derived());
      
      data.liMi[i] = model.jointPlacements[i]*jdata.M();
      
      data.a_gf[i] = data.liMi[i].actInv(data.a_gf[(size_t) parent]);
      data.f[i] = model.inertias[i]*data.a_gf[i];
    }
    
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl>
  struct ComputeGeneralizedGravityBackwardStep
  : public fusion::JointVisitorBase< ComputeGeneralizedGravityBackwardStep<Scalar,Options,JointCollectionTpl> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;

    typedef boost::fusion::vector<const Model &,
                                  Data &
                                  > ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      
      jmodel.jointVelocitySelector(data.g) = jdata.S().transpose()*data.f[i];
      if(parent>0) data.f[(size_t) parent] += data.liMi[i].act(data.f[i]);
    }
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::TangentVectorType &
  computeGeneralizedGravity(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                            DataTpl<Scalar,Options,JointCollectionTpl> & data,
                            const Eigen::MatrixBase<ConfigVectorType> & q)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    data.a_gf[0] = -model.gravity;
    
    typedef ComputeGeneralizedGravityForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType> Pass1;
    for(JointIndex i=1; i<(JointIndex)model.njoints; ++i)
    {
      Pass1::run(model.joints[i],data.joints[i],
                 typename Pass1::ArgsType(model,data,q.derived()));
    }
    
    typedef ComputeGeneralizedGravityBackwardStep<Scalar,Options,JointCollectionTpl> Pass2;
    for(JointIndex i=(JointIndex)(model.njoints-1); i>0; --i)
    {
      Pass2::run(model.joints[i],data.joints[i],
                 typename Pass2::ArgsType(model,data));
    }
    
    return data.g;
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType>
  struct CoriolisMatrixForwardStep
  : public fusion::JointVisitorBase< CoriolisMatrixForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &,
                                  const ConfigVectorType &,
                                  const TangentVectorType &
                                  > ArgsType;

    template<typename JointModel>
    static void algo(const pinocchio::JointModelBase<JointModel> & jmodel,
                     pinocchio::JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q,
                     const Eigen::MatrixBase<TangentVectorType> & v)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];

      jmodel.calc(jdata.derived(),q.derived(),v.derived());

      data.liMi[i] = model.jointPlacements[i]*jdata.M();
      if(parent>0) data.oMi[i] = data.oMi[parent] * data.liMi[i];
      else data.oMi[i] = data.liMi[i];
      
      // express quantities in the world frame
      data.oYcrb[i] = data.oMi[i].act(model.inertias[i]);
      
      data.v[i] = jdata.v();
      if(parent>0) data.v[i] += data.liMi[i].actInv(data.v[parent]);
      data.ov[i] = data.oMi[i].act(data.v[i]);
      
      // computes S expressed at the world frame
      typedef typename SizeDepType<JointModel::NV>::template ColsReturn<typename Data::Matrix6x>::Type ColsBlock;
      ColsBlock Jcols = jmodel.jointCols(data.J);
      Jcols = data.oMi[i].act(jdata.S()); // collection of S expressed at the world frame

      // computes vxS expressed at the world frame
      ColsBlock dJcols = jmodel.jointCols(data.dJ);
      motionSet::motionAction(data.ov[i],Jcols,dJcols);

      // computes vxI
      typedef typename Data::Inertia Inertia;
      Inertia::vxi(data.ov[i],data.oYcrb[i],data.vxI[i]);
    }

  };

  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl>
  struct CoriolisMatrixBackwardStep
  : public fusion::JointVisitorBase< CoriolisMatrixBackwardStep<Scalar,Options,JointCollectionTpl> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &
                                  > ArgsType;

    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     const Model & model,
                     Data & data)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      typename Data::RowMatrix6 & M6tmpR = data.M6tmpR;

      typedef typename SizeDepType<JointModel::NV>::template ColsReturn<typename Data::Matrix6x>::Type ColsBlock;
      ColsBlock dJcols = jmodel.jointCols(data.dJ);
      ColsBlock Jcols = jmodel.jointCols(data.J);
      
      motionSet::inertiaAction(data.oYcrb[i],dJcols,jmodel.jointCols(data.dFdv));
      jmodel.jointCols(data.dFdv) += data.vxI[i] * Jcols;

      data.C.block(jmodel.idx_v(),jmodel.idx_v(),jmodel.nv(),data.nvSubtree[i]).noalias()
      = Jcols.transpose()*data.dFdv.middleCols(jmodel.idx_v(),data.nvSubtree[i]);
      
      lhsInertiaMult(data.oYcrb[i],Jcols.transpose(),M6tmpR.topRows(jmodel.nv()));
      for(int j = data.parents_fromRow[(JointIndex)jmodel.idx_v()];j >= 0; j = data.parents_fromRow[(JointIndex)j])
        data.C.middleRows(jmodel.idx_v(),jmodel.nv()).col(j).noalias() = M6tmpR.topRows(jmodel.nv()) * data.dJ.col(j);

      M6tmpR.topRows(jmodel.nv()).noalias() = Jcols.transpose() * data.vxI[i];
      for(int j = data.parents_fromRow[(JointIndex)jmodel.idx_v()];j >= 0; j = data.parents_fromRow[(JointIndex)j])
        data.C.middleRows(jmodel.idx_v(),jmodel.nv()).col(j) += M6tmpR.topRows(jmodel.nv()) * data.J.col(j);

      if(parent>0)
      {
        data.oYcrb[parent] += data.oYcrb[i];
        data.vxI[parent] += data.vxI[i];
      }
      
    }
    
    template<typename Min, typename Mout>
    static void lhsInertiaMult(const typename Data::Inertia & Y,
                               const Eigen::MatrixBase<Min> & J,
                               const Eigen::MatrixBase<Mout> & F)
    {
      Mout & F_ = PINOCCHIO_EIGEN_CONST_CAST(Mout,F);
      motionSet::inertiaAction(Y,J.derived().transpose(),F_.transpose());
    }
  };
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::MatrixXs &
  computeCoriolisMatrix(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                        DataTpl<Scalar,Options,JointCollectionTpl> & data,
                        const Eigen::MatrixBase<ConfigVectorType> & q,
                        const Eigen::MatrixBase<TangentVectorType> & v)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq);
    assert(v.size() == model.nv);
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;
    
    typedef CoriolisMatrixForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType> Pass1;
    for(JointIndex i=1; i<(JointIndex)model.njoints; ++i)
    {
      Pass1::run(model.joints[i],data.joints[i],
                 typename Pass1::ArgsType(model,data,q.derived(),v.derived()));
    }
    
    typedef CoriolisMatrixBackwardStep<Scalar,Options,JointCollectionTpl> Pass2;
    for(JointIndex i=(JointIndex)(model.njoints-1); i>0; --i)
    {
      Pass2::run(model.joints[i],
                 typename Pass2::ArgsType(model,data));
    }
    
    return data.C;
  }
  
} // namespace pinocchio

/// @endcond

#endif // ifndef __pinocchio_rnea_hxx__
