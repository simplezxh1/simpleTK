#include "MPRMaker.h"
#include <vtkDICOMMetaData.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkImageProperty.h>
#include <vtkCamera.h>
#include <vtkInformation.h>
//#include "image.h"
//#include "series.h"
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkImageActor.h>
#include <vtkScalarsToColors.h>
#include <vtkMatrix4x4.h>
#include <vtkImageResliceMapper.h>
#include <vtkTextProperty.h>


//-----------------------------------------------------------------------------
void MPRMaker::SetRenderWindows(const vtkSmartPointer<vtkRenderWindow>& t_sagittalWindow,
	const vtkSmartPointer<vtkRenderWindow>& t_coronalWindow,
	const vtkSmartPointer<vtkRenderWindow>& t_axialWindow)
{
	m_renderWindow[0] = t_sagittalWindow;
	m_renderWindow[1] = t_coronalWindow;
	m_renderWindow[2] = t_axialWindow;
}

//-----------------------------------------------------------------------------
vtkImageReslice* MPRMaker::getOriginalValueImageReslice(const int t_plane)
{
	m_originalValuesReslicer[t_plane]->SetResliceAxes(m_reslicer[t_plane]->GetResliceAxes());
	return m_originalValuesReslicer[t_plane];
}

//-----------------------------------------------------------------------------
double MPRMaker::getCenterSliceZPosition(const int t_plane) const
{
	int* const extent = m_reader->GetOutput()->GetExtent();
	return 0.5 * (extent[t_plane * 2] + extent[t_plane * 2 + 1]);
}

//-----------------------------------------------------------------------------
void MPRMaker::create3DMatrix()
{
	// if (!m_series)
	// {
	// 	m_series = m_image->getParentObject();
	// }
	//
	// m_reader =
	// 	m_image && m_image->getIsMultiFrame()
	// 	? m_image->getImageReader()
	// 	: m_series->getReaderForAllSingleFrameImages();
}

//-----------------------------------------------------------------------------
void MPRMaker::createMPR(vtkSmartPointer<vtkDICOMReader> reader)
{
	this->m_reader = reader;
	createMprViews();
}

//-----------------------------------------------------------------------------
void MPRMaker::resetMatrixesToInitialPosition()
{
	setInitialMatrix();
	for (auto i = 0; i < 3; ++i)
	{
		setMiddleSlice(i, m_reader);
	}
}

//-----------------------------------------------------------------------------
void MPRMaker::resetWindowLevel()
{
	const double level =
		m_reader->GetMetaData()->Get(DC::WindowCenter).AsInt();
	const double window =
		m_reader->GetMetaData()->Get(DC::WindowWidth).AsInt();
	if (!m_colorMap)
	{
		m_colorMap = vtkSmartPointer<vtkScalarsToColors>::New();
	}
	m_colorMap->SetRange(level - 0.5 * window, level + 0.5 * window);
}

//-----------------------------------------------------------------------------
void MPRMaker::initialize()
{
	for (auto i = 0; i < 3; ++i)
	{
		m_reslicer[i] = vtkSmartPointer<vtkImageResliceToColors>::New();
		m_originalValuesReslicer[i] = vtkSmartPointer<vtkImageReslice>::New();
		mTextActor[i] = vtkSmartPointer<vtkTextActor>::New();
	}
	setInitialMatrix();
}

//-----------------------------------------------------------------------------
void MPRMaker::setInitialMatrix()
{
	vtkNew<vtkMatrix4x4> axialMatrix;
	axialMatrix->DeepCopy(m_axialMatrix);
	m_reslicer[2]->SetResliceAxes(axialMatrix);
	vtkNew<vtkMatrix4x4> sagittalMatrix;																									
	sagittalMatrix->DeepCopy(m_sagittalMatrix);
	m_reslicer[0]->SetResliceAxes(sagittalMatrix);
	vtkNew<vtkMatrix4x4> coronalMatrix;
	coronalMatrix->DeepCopy(m_coronalMatrix);
	m_reslicer[1]->SetResliceAxes(coronalMatrix);
}

//-----------------------------------------------------------------------------
void MPRMaker::createMprViews()
{
	for (auto i = 0; i < 3; i++)
	{
		renderPlaneOffScreen(i);
	}
}

//-----------------------------------------------------------------------------
void MPRMaker::setMiddleSlice(const int t_plane, const vtkSmartPointer<vtkDICOMReader>& t_reader)
{
	double spacing[3];
	double origin[3];
	double center[3];
	auto* const extent = m_reader->GetOutput()->GetExtent();
	t_reader->GetOutputInformation(0)->Get(
		vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), extent);
	t_reader->GetOutput()->GetSpacing(spacing);
	t_reader->GetOutput()->GetOrigin(origin);
	center[0] = origin[0] + spacing[0] * 0.5 * (extent[0] + extent[1]);
	center[1] = origin[1] + spacing[1] * 0.5 * (extent[2] + extent[3]);
	center[2] = origin[2] + spacing[2] * 0.5 * (extent[4] + extent[5]);
	m_reslicer[t_plane]->GetResliceAxes()->SetElement(0, 3, center[0]);
	m_reslicer[t_plane]->GetResliceAxes()->SetElement(1, 3, center[1]);
	m_reslicer[t_plane]->GetResliceAxes()->SetElement(2, 3, center[2]);
}

//-----------------------------------------------------------------------------
void MPRMaker::renderPlaneOffScreen(const int t_plane)
{
	 const double level = m_initialWindow == 0
	 	? m_reader->GetMetaData()->Get(DC::WindowCenter).AsInt()
	 	: m_initialWindow;
	 const double window = m_initialLevel == 0 ? m_reader->GetMetaData()->Get(DC::WindowWidth).AsInt() : m_initialLevel;

	setMiddleSlice(t_plane, m_reader);
	if (!m_colorMap)
	{
		m_colorMap = vtkSmartPointer<vtkScalarsToColors>::New();
		m_colorMap->SetRange(level - 0.5 * window, level + 0.5 * window);
	}
 	m_reslicer[t_plane]->SetInputData(m_reader->GetOutput());
	m_reslicer[t_plane]->BypassOff();
	m_reslicer[t_plane]->SetInformationInput(m_reader->GetOutput());
	m_reslicer[t_plane]->SetOutputDimensionality(2);
	m_reslicer[t_plane]->SetSlabNumberOfSlices(0);
	m_reslicer[t_plane]->SetLookupTable(m_colorMap);
	m_reslicer[t_plane]->SetOutputFormatToRGB();
	m_reslicer[t_plane]->Update();
	m_originalValuesReslicer[t_plane]->SetInputData(m_reader->GetOutput());
	m_originalValuesReslicer[t_plane]->SetInformationInput(m_reader->GetOutput());
	m_originalValuesReslicer[t_plane]->SetOutputDimensionality(2);
	m_originalValuesReslicer[t_plane]->SetResliceAxes(m_reslicer[t_plane]->GetResliceAxes());
	m_originalValuesReslicer[t_plane]->Update();

	 switch (t_plane)
	 {
	 case 0:
		 mTextActor[t_plane]->SetInput("Sagittal");
		 mTextActor[t_plane]->GetTextProperty()->SetColor(0, 1, 0);

		break;
	 case 1:
		 mTextActor[t_plane]->SetInput("Coronal");
		 mTextActor[t_plane]->GetTextProperty()->SetColor(0, 0, 1);
		 break;
	 case 2:
		 mTextActor[t_plane]->SetInput("Axial");
		 mTextActor[t_plane]->GetTextProperty()->SetColor(1, 0, 0);
		 break;
		default:
			break;
	 }

	//mTextActor[t_plane]->SetDisplayPosition(0,220);
	mTextActor[t_plane]->GetTextProperty()->SetFontSize(20);
	vtkNew<vtkImageSlice> slice;
	vtkNew<vtkRenderer> renderer;
	vtkNew<vtkImageResliceMapper> mapper;
	mapper->SeparateWindowLevelOperationOff();
	mapper->SetInputConnection(m_reslicer[t_plane]->GetOutputPort());
	slice->GetProperty()->SetInterpolationTypeToCubic();
	slice->SetMapper(mapper);
	renderer->AddViewProp(slice);
	renderer->AddActor(mTextActor[t_plane]);
	renderer->SetBackground(0, 0, 0);
	renderer->GetActiveCamera()->SetParallelProjection(1);
	renderer->ResetCamera();
	m_renderWindow[t_plane]->AddRenderer(renderer);
	m_renderWindow[t_plane]->Render();
}
