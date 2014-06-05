/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

#include "PlusConfigure.h"
#include "vtkDataCollector.h"
#include "vtkPlusStealthLinkCommand.h"
#include "StealthLink\vtkStealthLinkTracker.h"

#include "vtkImageData.h"
#include "vtkDICOMImageReader.h"
#include "vtkObjectFactory.h"
#include "vtkPlusChannel.h"
#include "vtkPlusCommandProcessor.h"
#include "vtkTrackedFrameList.h"
#include "vtkVolumeReconstructor.h"
#include "vtkVirtualVolumeReconstructor.h"
#include <vtkImageFlip.h>
#include <vtkPointData.h>
#include <vtkDirectory.h>

static const char GET_STEALTHLINK_EXAM_DATA_CMD[]="ExamData";
static const char GET_STEALTHLINK_REGISTRATION_DATA_CMD[]="RegistrationData";

vtkStandardNewMacro( vtkPlusStealthLinkCommand );

//----------------------------------------------------------------------------
vtkPlusStealthLinkCommand::vtkPlusStealthLinkCommand()
: PatientName(NULL)
, PatientId(NULL)
, StealthLinkDeviceId(NULL)
, DicomImagesOutputDirectory(NULL)
, VolumeEmbeddedTransformToFrame(NULL)
, KeepReceivedDicomFiles(false)
{
	this->FrameToExamTransform = vtkSmartPointer<vtkMatrix4x4>::New();
}

//----------------------------------------------------------------------------
vtkPlusStealthLinkCommand::~vtkPlusStealthLinkCommand()
{
  SetPatientName(NULL);
  SetStealthLinkDeviceId(NULL);
  SetPatientId(NULL);
	SetDicomImagesOutputDirectory(NULL);
	SetVolumeEmbeddedTransformToFrame(NULL);
	SetKeepReceivedDicomFiles(false);
  FrameToExamTransform->Identity();
}

//----------------------------------------------------------------------------
void vtkPlusStealthLinkCommand::PrintSelf( ostream& os, vtkIndent indent )
{
  this->Superclass::PrintSelf( os, indent );
}

//----------------------------------------------------------------------------
void vtkPlusStealthLinkCommand::GetCommandNames(std::list<std::string> &cmdNames)
{ 
  cmdNames.clear(); 
  cmdNames.push_back(GET_STEALTHLINK_EXAM_DATA_CMD);
  cmdNames.push_back(GET_STEALTHLINK_REGISTRATION_DATA_CMD);
}

//----------------------------------------------------------------------------
std::string vtkPlusStealthLinkCommand::GetDescription(const char* commandName)
{ 
  std::string desc;
  if (commandName==NULL || STRCASECMP(commandName, GET_STEALTHLINK_EXAM_DATA_CMD))
  {
    desc+=GET_STEALTHLINK_EXAM_DATA_CMD;
    desc+=": Acquire the exam data from the StealthLink Server. The exam data contains the image being displayed on the StealthLink Server. The 3D volume will be constructed using these images";
  }
  if (commandName==NULL || STRCASECMP(commandName, GET_STEALTHLINK_REGISTRATION_DATA_CMD))
  {
    desc+=GET_STEALTHLINK_REGISTRATION_DATA_CMD;
    desc+="Acquire the registration data from the StealthLink Server. The data contains the transformation matrix between the image being displayed and the reference frame.";
  }
  return desc;
}
//----------------------------------------------------------------------------
void vtkPlusStealthLinkCommand::SetNameToGetExam() { SetName(GET_STEALTHLINK_EXAM_DATA_CMD); }
//----------------------------------------------------------------------------
void vtkPlusStealthLinkCommand::SetNameToGetRegistration() { SetName(GET_STEALTHLINK_REGISTRATION_DATA_CMD); }
//----------------------------------------------------------------------------
void vtkPlusStealthLinkCommand::SetKeepReceivedDicomFiles(bool keepReceivedDicomFiles) { this->KeepReceivedDicomFiles = keepReceivedDicomFiles; }
//----------------------------------------------------------------------------
bool vtkPlusStealthLinkCommand::GetKeepReceivedDicomFiles() { return this->KeepReceivedDicomFiles; }
//----------------------------------------------------------------------------
PlusStatus vtkPlusStealthLinkCommand::ReadConfiguration(vtkXMLDataElement* aConfig)
{  
	
  if (vtkPlusCommand::ReadConfiguration(aConfig)!=PLUS_SUCCESS)
  {
    return PLUS_FAIL;
  }
  if(aConfig->GetAttribute("DicomImagesOutputDirectory"))
  {
    this->SetDicomImagesOutputDirectory(aConfig->GetAttribute("DicomImagesOutputDirectory"));
  }
	else
	{
		LOG_INFO("The dicom images from stealthlink will be saved into the: " << vtkPlusConfig::GetInstance()->GetOutputDirectory() << "/StealthLinkDicomOutput");
		std::string dicomImagesDefaultOutputDirectory = vtkPlusConfig::GetInstance()->GetOutputDirectory() +  std::string("/StealthLinkDicomOutput");
	  this->SetDicomImagesOutputDirectory(dicomImagesDefaultOutputDirectory.c_str());
	}
  if(aConfig->GetAttribute("StealthLinkDeviceId"))
  {
	  this->SetStealthLinkDeviceId(aConfig->GetAttribute("StealthLinkDeviceId"));
  }
	if(aConfig->GetAttribute("VolumeEmbeddedTransformToFrame"))
	{
		this->SetVolumeEmbeddedTransformToFrame(aConfig->GetAttribute("VolumeEmbeddedTransformToFrame"));
	}
	else
	{
		LOG_INFO("The dicom images from stealthlink will be saved represented in Ras coordinate system");
	  this->SetVolumeEmbeddedTransformToFrame("Ras");
	}
	if(aConfig->GetAttribute("KeepReceivedDicomFiles"))
	{
		std::string stringTrue("true");
		if(!stringTrue.compare(aConfig->GetAttribute("KeepReceivedDicomFiles")))
		{
			this->SetKeepReceivedDicomFiles(true);
		}
		else
		{
			this->SetKeepReceivedDicomFiles(false);
		}
	}
  return PLUS_SUCCESS;
}
//----------------------------------------------------------------------------
PlusStatus vtkPlusStealthLinkCommand::WriteConfiguration(vtkXMLDataElement* aConfig)
{  
  if(vtkPlusCommand::WriteConfiguration(aConfig)!=PLUS_SUCCESS)
  {
    return PLUS_FAIL;
  }
  if(this->GetStealthLinkDeviceId()!=NULL)
  {
    aConfig->SetAttribute("StealthLinkDeviceId",this->GetStealthLinkDeviceId());     
  }
  if(this->GetDicomImagesOutputDirectory()!=NULL)
  {
    aConfig->SetAttribute("DicomImagesOutputDirectory",this->GetDicomImagesOutputDirectory());     
  }
	if(this->GetVolumeEmbeddedTransformToFrame()!=NULL)
	{
		aConfig->SetAttribute("VolumeEmbeddedTransformToFrame",this->GetVolumeEmbeddedTransformToFrame());
	}
	if(this->GetKeepReceivedDicomFiles()==true)
	{
		aConfig->SetAttribute("KeepReceivedDicomFiles","true");
	}
	else
	{
		aConfig->SetAttribute("KeepReceivedDicomFiles","false");
	}
  return PLUS_SUCCESS;
}
//----------------------------------------------------------------------------
bool IsMatrixIdentityMatrix(vtkMatrix4x4* matrix)
{
	for(int i=0;i<4;i++)
	{
		for(int j=0;j<4;j++)
		{
			if(i==j)
			{
				if(matrix->GetElement(i,j)!=1)
					return false;
			}
			else
			{
				if(matrix->GetElement(i,j)!=0)
					return false;
			}
		}
	}
	return true;
}
//----------------------------------------------------------------------------
PlusStatus vtkPlusStealthLinkCommand::DeleteDicomImageOutputDirectory(std::string examImageDirectory)
{
	vtkDirectory::DeleteDirectory(examImageDirectory.c_str());
  vtkSmartPointer<vtkDirectory> dicomDirectory = vtkSmartPointer<vtkDirectory>::New();
	if(dicomDirectory->Open(examImageDirectory.c_str()))
	{
	  vtkDirectory::DeleteDirectory(examImageDirectory.c_str());
		examImageDirectory.clear();
		if(dicomDirectory->Open(this->GetDicomImagesOutputDirectory()))
		{
		  if(dicomDirectory->GetNumberOfFiles() == 2)
			{
			  std::string file1(".");
				std::string file2("..");
				if(file1.compare(dicomDirectory->GetFile(0)) == 0 && file2.compare(dicomDirectory->GetFile(1)) ==0)
				{
				  vtkDirectory::DeleteDirectory(this->GetDicomImagesOutputDirectory());
					this->SetDicomImagesOutputDirectory(NULL);
				}
			}
		}
		else
		{
			LOG_ERROR("Cannot open the folder: " << this->GetDicomImagesOutputDirectory());
			return PLUS_FAIL;
		}
	}
	else
	{
		LOG_ERROR("Cannot open the folder: " << examImageDirectory);
		return PLUS_FAIL;
	}
	return PLUS_SUCCESS;
}
//----------------------------------------------------------------------------
PlusStatus vtkPlusStealthLinkCommand::Execute()
{  
  LOG_DEBUG("vtkPlusStealthLinkCommand::Execute: "<<(this->Name?this->Name:"(undefined)")
    <<", device: "<<(this->StealthLinkDeviceId==NULL?"(undefined)":this->StealthLinkDeviceId) );
  //std::cout << "ID= " << this->StealthLinkDeviceId << "\n";
  if (this->Name==NULL)
  {
		this->QueueStringResponse("StealthLink command failed, no command name specified",PLUS_FAIL);
    return PLUS_FAIL;
  }

  vtkStealthLinkTracker* stealthLinkDevice = GetStealthLinkDevice();
  if (stealthLinkDevice==NULL)
  {
		this->QueueStringResponse(std::string("StealthLink command failed: device ")
      +(this->StealthLinkDeviceId==NULL?"(undefined)":this->StealthLinkDeviceId)+" is not found",PLUS_FAIL);
    return PLUS_FAIL;
  }

  if (STRCASECMP(this->Name, GET_STEALTHLINK_EXAM_DATA_CMD)==0)
  {
    LOG_INFO("Acquiring the exam data from StealthLink Server: Device ID: "<<GetStealthLinkDeviceId());
		
		if(!stealthLinkDevice->UpdateCurrentExam())
		{
			return PLUS_FAIL;
		}
		std::string patientName;
		std::string patientId;
		if(!stealthLinkDevice->GetPatientName(patientName))
	  {
		  return PLUS_FAIL;
 	  }
 	  if(!stealthLinkDevice->GetPatientId(patientId))
	  {
		  return PLUS_FAIL;
	  }
		stealthLinkDevice->SetDicomImagesOutputDirectory(this->GetDicomImagesOutputDirectory());

		SetPatientName(patientName.c_str());
	  SetPatientId(patientId.c_str());
		std::string imageId(std::string(this->GetStealthLinkDeviceId())+std::string("_")+this->GetPatientName());
		stealthLinkDevice->UpdateTransformRepository(this->CommandProcessor->GetPlusServer()->GetTransformRepository());
		vtkSmartPointer<vtkImageData> imageData = vtkSmartPointer<vtkImageData>::New();
		vtkSmartPointer<vtkMatrix4x4> ijkToReferenceTransform = vtkSmartPointer<vtkMatrix4x4>::New();
		stealthLinkDevice->GetImage(imageId, std::string(this->GetVolumeEmbeddedTransformToFrame()),imageData,ijkToReferenceTransform);

		
	 
		//if(this->GetKeepReceivedDicomFiles() == false)
		//{
			//this->DeleteDicomImageOutputDirectory(examImageDirectory);
		//}
		std::string resultMessage;
		PlusStatus status = ProcessImageReply(imageId,imageData,ijkToReferenceTransform,resultMessage);
		this->QueueStringResponse("Volume sending completed succesfully: "+resultMessage,status);
    return PLUS_SUCCESS;
  }
  else if (STRCASECMP(this->Name, GET_STEALTHLINK_REGISTRATION_DATA_CMD)==0)
  {    
    LOG_INFO("Acquiring the registration data from StealthLink Server: Device ID: "<<GetStealthLinkDeviceId());
	
	//if(stealthLinkDevice->UpdateCurrentRegistration()) TODO - Update is protected now so change the name of the function used here
	//{	
	//	return PLUS_FAIL;
	//}
    this->QueueStringResponse("Registration data fromn StealthLink, device: "+std::string(GetStealthLinkDeviceId()),PLUS_SUCCESS);
    return PLUS_SUCCESS;
  }
	this->QueueStringResponse("vtkPlusStealthLinkCommand::Execute: failed, unknown command name: "+std::string(this->Name),PLUS_SUCCESS);
  return PLUS_FAIL;
} 
//----------------------------------------------------------------------------
PlusStatus vtkPlusStealthLinkCommand::ProcessImageReply(const std::string& imageId, vtkImageData* volumeToSend,vtkMatrix4x4* imageToReferenceOrientationMatrix,std::string& resultMessage)
{
  LOG_DEBUG("Send image to client through OpenIGTLink");
	vtkSmartPointer<vtkPlusCommandImageResponse> imageResponse=vtkSmartPointer<vtkPlusCommandImageResponse>::New();
	this->CommandResponseQueue.push_back(imageResponse);
	imageResponse->SetClientId(this->ClientId);
	imageResponse->SetImageName(imageId);
  imageResponse->SetImageData(volumeToSend);
	imageResponse->SetImageToReferenceTransform(imageToReferenceOrientationMatrix);
  LOG_INFO("Send reconstructed volume to client through OpenIGTLink");
	resultMessage.clear();
  resultMessage=std::string(", volume sent as: ")+imageResponse->GetImageName();
  return PLUS_SUCCESS;
}
//----------------------------------------------------------------------------
vtkStealthLinkTracker* vtkPlusStealthLinkCommand::GetStealthLinkDevice()
{
  vtkDataCollector* dataCollector=GetDataCollector();
  if (dataCollector==NULL)
  {
    LOG_ERROR("Data collector is invalid");    
    return NULL;
  }
  vtkStealthLinkTracker *stealthLinkDevice=NULL;
  if (GetStealthLinkDeviceId()!=NULL)
  {
    // Reconstructor device ID is specified
    vtkPlusDevice* device=NULL;
    if (dataCollector->GetDevice(device, GetStealthLinkDeviceId())!=PLUS_SUCCESS)
    {
      LOG_ERROR("No StealthLink device has been found by the name "<<this->GetStealthLinkDeviceId());
      return NULL;
    }
    // device found
    stealthLinkDevice = vtkStealthLinkTracker::SafeDownCast(device);
    if (stealthLinkDevice==NULL)
    {
      // wrong type
      LOG_ERROR("The specified device "<<GetStealthLinkDeviceId()<<" is not StealthLink Device");
      return NULL;
    }
  }
  else
  {
    // No volume reconstruction device id is specified, auto-detect the first one and use that
    for( DeviceCollectionConstIterator it = dataCollector->GetDeviceConstIteratorBegin(); it != dataCollector->GetDeviceConstIteratorEnd(); ++it )
    {
	    stealthLinkDevice = vtkStealthLinkTracker::SafeDownCast(*it);
      if (stealthLinkDevice!=NULL)
      {      
        // found a recording device
		    SetStealthLinkDeviceId(stealthLinkDevice->GetDeviceId());
        break;
      }
    }
    if (stealthLinkDevice==NULL)
    {
      LOG_ERROR("No StealthLink Device has been found");
      return NULL;
    }
  }  
  return stealthLinkDevice;
}
