//====================================================================
//  AIDA Detector description implementation
//  for LDC AHCAL Endcap
//--------------------------------------------------------------------
//
//  Author     : S.Lu
//  F. Gaede, DESY :  v01 : prepare for multi segmentation
//     18.04.2017            - copied from SHcalSc04_Barrel_v01.cpp
//                           - add optional parameter <subsegmentation key="" value=""/>
//                             defines the segmentation to be used in reconstruction
//
// Basic idea:
// 1. Create the Hcal Endcap module envelope (16 modules).
//    Note: with default material Steel235.
//    
// 2. Create the Hcal Endcap Chamber(i.e. Layer) for each module.
//    Create the Layer with slices (Polystyrene,Cu,FR4,air).
//    Place each slice into the chamber with the right position,
//    And registry the IDs for slice
//
// 3. Place the same Layer into the endcap module envelope.
//    It will be repeated repeat 48 times.
//    And registry the IDs for layer, and endcapID.
//
// 4. Place the endcap module into the world volume,
//    with the right position and rotation.
//    And registry the IDs for stave,module and endcapID.
//
// 5. Customer material FR4 and Steel235 defined in materials.xml
//
//====================================================================
#include "DD4hep/DetFactoryHelper.h"
#include "DD4hep/DetType.h"
#include "XML/Layering.h"
#include "XML/Utilities.h"
#include "DDRec/DetectorData.h"
#include "DDSegmentation/Segmentation.h"
#include "DDSegmentation/MultiSegmentation.h"
#include "LcgeoExceptions.h"

using namespace std;
using namespace DD4hep;
using namespace DD4hep::Geometry;


static Ref_t create_detector(LCDD& lcdd, xml_h element, SensitiveDetector sens)  {
  xml_det_t   x_det     = element;
  Layering    layering(x_det);
  xml_dim_t   dim         = x_det.dimensions();
  string      det_name    = x_det.nameStr();
  //unused: string      det_type    = x_det.typeStr();
  Material    air         = lcdd.air();
  Material    stavesMaterial    = lcdd.material(x_det.materialStr());
  int         numSides    = dim.numsides();

  int           det_id    = x_det.id();

  DetElement   sdet(det_name,det_id);
 
  // --- create an envelope volume and position it into the world ---------------------

  Volume envelope = XML::createPlacedEnvelope( lcdd,  element , sdet ) ;
  
  sdet.setTypeFlag( DetType::CALORIMETER |  DetType::ENDCAP  | DetType::HADRONIC ) ;

  if( lcdd.buildType() == BUILD_ENVELOPE ) return sdet ;
  //-----------------------------------------------------------------------------------

  sens.setType("calorimeter");

  DetElement    stave_det("module0stave0",det_id);
 
  // The way to reaad constant from XML/LCDD file.
  double      Hcal_radiator_thickness          = lcdd.constant<double>("Hcal_radiator_thickness");
  double      Hcal_lateral_structure_thickness = lcdd.constant<double>("Hcal_lateral_structure_thickness");
  double      Hcal_endcap_layer_air_gap        = lcdd.constant<double>("Hcal_endcap_layer_air_gap");
  double      Hcal_layer_air_gap               = lcdd.constant<double>("Hcal_layer_air_gap");

  //double      Hcal_cells_size                  = lcdd.constant<double>("Hcal_cells_size");
  double      HcalEndcap_inner_radius          = lcdd.constant<double>("HcalEndcap_inner_radius");
  double      HcalEndcap_outer_radius          = lcdd.constant<double>("HcalEndcap_outer_radius");
  double      HcalEndcap_min_z                 = lcdd.constant<double>("HcalEndcap_min_z");
  double      HcalEndcap_max_z                 = lcdd.constant<double>("HcalEndcap_max_z");
  
  Readout readout = sens.readout();
  Segmentation seg = readout.segmentation();
  

  BitField64& encoder = *seg.decoder() ;
  encoder.setValue(0) ;
  
  //    we first have to check whether a multi segmentation is used and then, if so, we
  //    will check the <subsegmentation key="" value=""/> element, for which subsegmentation to use for filling the 
  //    DDRec:LayeredCalorimeterData information.

  // check if we have a multi segmentation :
  DD4hep::DDSegmentation::MultiSegmentation* multiSeg = 
    dynamic_cast< DD4hep::DDSegmentation::MultiSegmentation*>( seg.segmentation() ) ;
  
  int sensitive_slice_number = -1 ;

  if( multiSeg ){

    try{ 
      // check if we have an entry for the subsegmentation to be used 
      xml_comp_t segxml = x_det.child( _Unicode( subsegmentation ) ) ;

      std::string keyStr = segxml.attr<std::string>( _Unicode(key) ) ;
      int keyVal = segxml.attr<int>( _Unicode(value) )  ;

      encoder[ keyStr ] =  keyVal ;

      // if we have a multisegmentation that uses the slice as key, we need to know for the
      // computation of the layer parameters in LayeredCalorimeterData::Layer below
      if( keyStr == "slice" ){
	sensitive_slice_number = keyVal ;
      }

    } catch( std::runtime_error) {
      throw lcgeo::GeometryException(  "SHcalSc04_Endcaps_v01: Error: MultiSegmentation specified but no "
				       " <subsegmentation key="" value=""/> element defined for detector ! " ) ;
    }
  }
 
  //========== fill data for reconstruction ============================
  DDRec::LayeredCalorimeterData* caloData = new DDRec::LayeredCalorimeterData ;
  caloData->layoutType = DDRec::LayeredCalorimeterData::EndcapLayout ;
  caloData->inner_symmetry = 4  ; // hard code cernter box hole
  caloData->outer_symmetry = 0  ; // outer tube, or 8 for Octagun
  caloData->phi0 = 0 ;

  /// extent of the calorimeter in the r-z-plane [ rmin, rmax, zmin, zmax ] in mm.
  caloData->extent[0] = HcalEndcap_inner_radius ;
  caloData->extent[1] = HcalEndcap_outer_radius ;
  caloData->extent[2] = HcalEndcap_min_z ;
  caloData->extent[3] = HcalEndcap_max_z ;
  

  int endcapID = 0;
  for(xml_coll_t c(x_det.child(_U(dimensions)),_U(dimensions)); c; ++c) 
    {
      xml_comp_t l(c);
      
      double dim_x = l.attr<double>(_Unicode(dim_x));
      double dim_y = l.attr<double>(_Unicode(dim_y));
      double dim_z = l.attr<double>(_Unicode(dim_z));
      double pos_y = l.attr<double>(_Unicode(y_offset));
    
      // Hcal Endcap module shape
      double box_half_x= dim_x/2.0; // module width, all are same
      double box_half_y= dim_y/2.0; // total thickness, all are same
      double box_half_z= dim_z/2.0; // module length, changed, 
      
      double x_offset = box_half_x*numSides-box_half_x*endcapID*2.0-box_half_x;
      double y_offset = pos_y;
      
      Box    EndcapModule(box_half_x,box_half_y,box_half_z);
      
      // define the name of each endcap Module
      string envelopeVol_name   = det_name+_toString(endcapID,"_EndcapModule%d");
      
      Volume envelopeVol(envelopeVol_name,EndcapModule,stavesMaterial);
      
      // Set envelope volume attributes.
      envelopeVol.setAttributes(lcdd,x_det.regionStr(),x_det.limitsStr(),x_det.visStr());
      
      
      // ========= Create Hcal Chamber (i.e. Layers) ==============================
      // It will be the sub volume for placing the slices.
      // Itself will be placed into the Hcal Endcap modules envelope.
      // ==========================================================================
      
      // create Layer (air) and place the slices (Polystyrene,Cu,FR4,air) into it. 
      // place the Layer into the Hcal Endcap Modules envelope (stavesMaterial).
      
      // First Hcal Chamber position, start after first radiator
      double layer_pos_y     = - box_half_y + Hcal_radiator_thickness;                      
      
      // Create Hcal Endcap Chamber without radiator
      // Place into the Hcal Encap module envelope, after each radiator 
      int layer_num = 1;
      for(xml_coll_t m(x_det,_U(layer)); m; ++m)  {
	xml_comp_t   x_layer = m;
	int          repeat = x_layer.repeat();          // Get number of layers.

	double layer_thickness = layering.layer(layer_num)->thickness();
	string layer_name      = envelopeVol_name+"_layer";
	DetElement  layer(stave_det,layer_name,det_id);
	
	// Active Layer box & volume
	double active_layer_dim_x = box_half_x - Hcal_lateral_structure_thickness - Hcal_endcap_layer_air_gap;
	double active_layer_dim_y = layer_thickness/2.0;
	double active_layer_dim_z = box_half_z;// - Hcal_lateral_structure_thickness - Hcal_endcap_layer_air_gap;
	
	// Build chamber including air gap
	// The Layer will be filled with slices, 
	Volume layer_vol(layer_name, Box((active_layer_dim_x + Hcal_layer_air_gap),
					 active_layer_dim_y,active_layer_dim_z), air);



	encoder["layer"] = layer_num ;
	std::vector<double> cellSizeVector = seg.segmentation()->cellDimensions( encoder.getValue() ); 

	DDRec::LayeredCalorimeterData::Layer caloLayer ;
	caloLayer.cellSize0 = cellSizeVector[0];
	caloLayer.cellSize1 = cellSizeVector[1];
	
	// ========= Create sublayer slices =========================================
	// Create and place the slices into Layer
	// ==========================================================================
	
	// Create the slices (sublayers) within the Hcal Chamber.
	double slice_pos_y = -(layer_thickness / 2.0);
	int slice_number = 0;

	double nRadiationLengths=0.;
	double nInteractionLengths=0.;
	double thickness_sum=0;

	nRadiationLengths   = Hcal_radiator_thickness/(stavesMaterial.radLength());
	nInteractionLengths = Hcal_radiator_thickness/(stavesMaterial.intLength());
	thickness_sum       = Hcal_radiator_thickness;

	for(xml_coll_t k(x_layer,_U(slice)); k; ++k)  {
	  xml_comp_t x_slice = k;
	  string   slice_name      = layer_name + _toString(slice_number,"_slice%d");
	  double   slice_thickness = x_slice.thickness();
	  Material slice_material  = lcdd.material(x_slice.materialStr());
	  DetElement slice(layer,_toString(slice_number,"slice%d"),det_id);
	  
	  slice_pos_y += slice_thickness / 2.0;
	  
	  // Slice volume & box
	  Volume slice_vol(slice_name,Box(active_layer_dim_x,slice_thickness/2.0,active_layer_dim_z),slice_material);
	  
	  nRadiationLengths   += slice_thickness/(2.*slice_material.radLength());
	  nInteractionLengths += slice_thickness/(2.*slice_material.intLength());
	  thickness_sum       += slice_thickness/2;

	  // if we have a multisegmentation based on slices, we need to use the correct slice here
	  if ( ( sensitive_slice_number<0 && x_slice.isSensitive() ) || sensitive_slice_number == slice_number ) {

	    sens.setType("calorimeter");
	    slice_vol.setSensitiveDetector(sens);

	    //Store "inner" quantities
	    caloLayer.inner_nRadiationLengths = nRadiationLengths;
	    caloLayer.inner_nInteractionLengths = nInteractionLengths;
	    caloLayer.inner_thickness = thickness_sum;
	    //Store scintillator thickness
	    caloLayer.sensitive_thickness = slice_thickness;

	    //Reset counters to measure "outside" quantitites
	    nRadiationLengths=0.;
	    nInteractionLengths=0.;
	    thickness_sum = 0.;
	  }

	  nRadiationLengths += slice_thickness/(2.*slice_material.radLength());
	  nInteractionLengths += slice_thickness/(2.*slice_material.intLength());
	  thickness_sum += slice_thickness/2;

	  // Set region, limitset, and vis.
	  slice_vol.setAttributes(lcdd,x_slice.regionStr(),x_slice.limitsStr(),x_slice.visStr());
	  // slice PlacedVolume
	  PlacedVolume slice_phv = layer_vol.placeVolume(slice_vol,Position(0,slice_pos_y,0));
	  slice_phv.addPhysVolID("slice",slice_number);
	  
	  slice.setPlacement(slice_phv);
	  // Increment Z position for next slice.
	  slice_pos_y += slice_thickness / 2.0;
	  // Increment slice number.
	  ++slice_number;             
	}
	// Set region, limitset, and vis.
	layer_vol.setAttributes(lcdd,x_layer.regionStr(),x_layer.limitsStr(),x_layer.visStr());


	//Store "outer" quantities
	caloLayer.outer_nRadiationLengths = nRadiationLengths;
	caloLayer.outer_nInteractionLengths = nInteractionLengths;
	caloLayer.outer_thickness = thickness_sum;
	
	// ========= Place the Layer (i.e. Chamber) =================================
	// Place the Layer into the Hcal Endcap module envelope.
	// with the right position and rotation.
	// Registry the IDs (layer, stave, module).
	// Place the same layer 48 times into Endcap module
	// ==========================================================================
	
	for (int j = 0; j < repeat; j++)    {
	  
	  // Layer position in y within the Endcap Modules.
	  layer_pos_y += layer_thickness / 2.0;
	  
	  PlacedVolume layer_phv = envelopeVol.placeVolume(layer_vol,
							   Position(0,layer_pos_y,0));
	  // registry the ID of Layer, stave and module
	  layer_phv.addPhysVolID("layer",layer_num);

	  // then setPlacement for it.
	  layer.setPlacement(layer_phv);
	  
	  
	  //-----------------------------------------------------------------------------------------
	  if ( caloData->layers.size() < (unsigned int)repeat ) {

	    caloLayer.distance = HcalEndcap_min_z + box_half_y + layer_pos_y
	      - caloLayer.inner_thickness ; // Will be added later at "DDMarlinPandora/DDGeometryCreator.cc:226" to get center of sensitive element
	    caloLayer.absorberThickness = Hcal_radiator_thickness ;
	    
	    caloData->layers.push_back( caloLayer ) ;
	  }
	  //-----------------------------------------------------------------------------------------
	  
	  
	  // ===== Prepare for next layer (i.e. next Chamber) =========================
	  // Prepare the chamber placement position and the chamber dimension
	  // ==========================================================================
	  
	  // Increment the layer_pos_y
	  // Place Hcal Chamber after each radiator 
	  layer_pos_y += layer_thickness / 2.0;
	  layer_pos_y += Hcal_radiator_thickness;
	  ++layer_num;         
	}
	
	
      }
      
      
      // =========== Place Hcal Endcap envelope ===================================
      // Finally place the Hcal Endcap envelope into the world volume.
      // Registry the stave(up/down), module(left/right) and endcapID.
      // ==========================================================================
      
      // Acording to the number of staves and modules,
      // Place the same Hcal Endcap module volume into the world volume
      // with the right position and rotation.
      for(int stave_num=0;stave_num<2;stave_num++){
	
	double EndcapModule_pos_x = 0;
	double EndcapModule_pos_y = 0;
	double EndcapModule_pos_z = 0;
	double rot_EM = 0;

	double EndcapModule_center_pos_z = HcalEndcap_min_z + box_half_y;
	
	switch (stave_num)
	  {
	  case 0 : 
	    EndcapModule_pos_x = x_offset;
	    EndcapModule_pos_y = y_offset;
	    break;
	  case 1 : 
	    EndcapModule_pos_x = -x_offset;
	    EndcapModule_pos_y = -y_offset;
	    break;
	  }
	
	for(int module_num=0;module_num<2;module_num++) {

	  int module_id = (module_num==0)? 0:6;
	  
	  rot_EM = (module_id==0)?(-M_PI/2.0):(M_PI/2.0);
	  
	  EndcapModule_pos_z = (module_id==0)? -EndcapModule_center_pos_z:EndcapModule_center_pos_z;

	  PlacedVolume env_phv = envelope.placeVolume(envelopeVol,
						      Transform3D(RotationX(rot_EM),
								  Translation3D(EndcapModule_pos_x,
										EndcapModule_pos_y,
										EndcapModule_pos_z)));
	  env_phv.addPhysVolID("tower",endcapID);	  
	  env_phv.addPhysVolID("stave",stave_num);   // y: up /down
	  env_phv.addPhysVolID("module",module_id); // z: -/+ 0/6
	  env_phv.addPhysVolID("system",det_id);

	  DetElement sd = (module_num==0&&stave_num==0) ? stave_det : stave_det.clone(_toString(module_id,"module%d")+_toString(stave_num,"stave%d"));	  
	  sd.setPlacement(env_phv);	  

	}
	
      }

    endcapID++;
      
    }
  
  sdet.addExtension< DDRec::LayeredCalorimeterData >( caloData ) ;  
  
  return sdet;
}




DECLARE_DETELEMENT(SHcalSc04_Endcaps_v01, create_detector)
