#include "NCPluginFactory.hh"
#include "NCPhysicsModel.hh"

namespace NCPluginNamespace {

  class PluginScatter final : public NC::ProcImpl::ScatterIsotropicMat {
  public:

    //The factory wraps our custom PhysicsModel helper class in an NCrystal API
    //Scatter class.

    const char * name() const noexcept override
    {
      return NCPLUGIN_NAME_CSTR "Model";
    }

    PluginScatter( PhysicsModel && pm ) : m_pm(std::move(pm)) {}

    NC::CrossSect
    crossSectionIsotropic( NC::CachePtr&,
                           NC::NeutronEnergy ekin ) const override
    {
      return NC::CrossSect{ m_pm.calcCrossSection(ekin.dbl()) };
    }

    NC::ScatterOutcomeIsotropic
    sampleScatterIsotropic( NC::CachePtr&,
                            NC::RNG& rng,
                            NC::NeutronEnergy ekin ) const override
    {
      auto outcome = m_pm.sampleScatteringEvent( rng, ekin.dbl() );
      return { NC::NeutronEnergy{outcome.ekin_final},
               NC::CosineScatAngle{outcome.mu} };
    }

  private:
    PhysicsModel m_pm;
  };

}

const char * NCP::PluginFactory::name() const noexcept
{
  //Factory name. Keep this standardised form please:
  return NCPLUGIN_NAME_CSTR "Factory";
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Here follows the factory logic, for how the physics model provided by the  //
// plugin should be combined with existing models in NCrystal.                //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

NC::Priority
NCP::PluginFactory::query( const NC::FactImpl::ScatterRequest& cfg ) const
{
  //Must return value >0 if we should do something, and a value higher than
  //100 means that we take precedence over the standard NCrystal factory:
  if ( cfg.get_inelas()=="none"
       || cfg.get_inelas()=="0"
       || cfg.get_inelas()=="false"
       || cfg.get_inelas()=="sterile" )
    return NC::Priority::Unable;//inelastic disabled, never do anything.

  //Ok, we might be applicable. Load input data and check if is something we
  //want to handle:
  if ( ! PhysicsModel::isApplicable( cfg.info() ) )
    return NC::Priority::Unable;

  //Ok, all good. Tell the framework that we want to deal with this, with a
  //higher priority than the standard factory gives (which is 100):
  return NC::Priority{999};
}

NC::ProcImpl::ProcPtr
NCP::PluginFactory::produce( const NC::FactImpl::ScatterRequest& cfg ) const
{
  auto sc_ourmodel = NC::makeSO<PluginScatter>( PhysicsModel::createFromInfo( cfg.info() ) );
  auto sc_std = globalCreateScatter( cfg );
  //Combine and return:
  return combineProcs( sc_std, sc_ourmodel );
}
