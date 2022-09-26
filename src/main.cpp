#include "sciter-x.h"
#include "sciter-x-window.hpp"

#include "itkImage.h"

#include "itkGDCMSeriesFileNames.h"
#include "itkImageSeriesReader.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"

#include "itkGDCMImageIO.h"
#include "itkGDCMImageIOFactory.h"
#include "itkNrrdImageIOFactory.h"

#include "itkImageRegionIteratorWithIndex.h"
#include "itkStatisticsImageFilter.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkResampleImageFilter.h"
#include "itkIdentityTransform.h"
#include "itkRescaleIntensityImageFilter.h"

#include "openvdb/openvdb.h"

#include <thread>
#include <future>
#include <chrono>

int _convertDICOM(sciter::value dirName, sciter::value outputMeta, sciter::value callback)
{
    std::string _dirName = dirName.get<std::string>();
    itk::GDCMImageIOFactory::RegisterOneFactory();
    using NamesGeneratorType = itk::GDCMSeriesFileNames;
    std::wstring info;
    auto nameGenerator = NamesGeneratorType::New();

    nameGenerator->SetUseSeriesDetails(true);
    nameGenerator->AddSeriesRestriction("0008|0021");
    nameGenerator->SetGlobalWarningDisplay(false);
    nameGenerator->SetDirectory(_dirName);

    try
    {
        using SeriesIdContainer = std::vector<std::string>;
        const SeriesIdContainer &seriesUID = nameGenerator->GetSeriesUIDs();
        auto seriesItr = seriesUID.begin();
        auto seriesEnd = seriesUID.end();

        if (seriesItr == seriesEnd)
        {
            callback.call(0);
            return 0;
        }

        seriesItr = seriesUID.begin();
        while (seriesItr != seriesUID.end())
        {
            std::string seriesID = seriesItr->c_str();
            seriesItr++;

            using FileNamesContainer = std::vector<std::string>;
            FileNamesContainer fileNames = nameGenerator->GetFileNames(seriesID);

            using PixelType = float;
            using ScaleType = double;
            constexpr unsigned int Dimension = 3;
            using ImageType = itk::Image<PixelType, Dimension>;
            using ReaderType = itk::ImageSeriesReader<ImageType>;
            using ImageIOType = itk::GDCMImageIO;

            auto reader = ReaderType::New();
            auto dicomIO = ImageIOType::New();

            reader->SetImageIO(dicomIO);
            reader->SetFileNames(fileNames);
            reader->ForceOrthogonalDirectionOff(); // properly read CTs with gantry tilt
            reader->Update();

            ImageType::Pointer image = reader->GetOutput();
            ImageType::SpacingType inputSpacing = image->GetSpacing();
            ImageType::RegionType inputRegion = image->GetLargestPossibleRegion();
            ImageType::SizeType inputSize = inputRegion.GetSize();
            ImageType::PointType inputOrigin = image->GetOrigin();

            ImageType::SizeType outputSize;
            ImageType::SpacingType outputSpacing;
            ImageType::PointType outputOrigin = inputOrigin;

            for (unsigned int dim = 0; dim < Dimension; ++dim)
            {
                outputSpacing[dim] = 1;
                outputSize[dim] = static_cast<double>(inputSize[dim]) * static_cast<double>(inputSpacing[dim]);
            }

            using TransformType = itk::IdentityTransform<ScaleType, Dimension>;
            using LinearInterpolatorType = itk::LinearInterpolateImageFunction<ImageType, ScaleType>;
            using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
            auto transformer = TransformType::New();
            auto interpolator = LinearInterpolatorType::New();
            auto resampleFilter = ResampleFilterType::New();

            resampleFilter->SetInput(image);
            resampleFilter->SetTransform(transformer);
            resampleFilter->SetInterpolator(interpolator);
            resampleFilter->SetSize(outputSize);
            resampleFilter->SetOutputSpacing(outputSpacing);
            resampleFilter->SetOutputOrigin(outputOrigin);
            resampleFilter->Update();
            image = resampleFilter->GetOutput();

            using RescaleFilterType = itk::RescaleIntensityImageFilter<ImageType, ImageType>;
            auto rescalefilter = RescaleFilterType::New();
            // outputMeta.set_item("max",10000);
            sciter::value max = outputMeta.get<std::vector<std::string>>();
            callback.call(max);
            rescalefilter->SetInput(image);
            rescalefilter->SetOutputMinimum(outputMeta.get_item('min').get<float>());
            rescalefilter->SetOutputMaximum(outputMeta.get_item('max').get<float>());
            rescalefilter->Update();
            image = rescalefilter->GetOutput();

            // Initialize the OpenVDB library.  This must be called at least
            // once per program and may safely be called multiple times.
            openvdb::initialize();

            // Create an empty floating-point grid with background value 0.
            openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create();

            // Name the grid "density".
            grid->setName("density");

            // Get an accessor for coordinate-based access to voxels.
            openvdb::FloatGrid::Accessor accessor = grid->getAccessor();

            // Define a coordinate with large signed indices.
            openvdb::Coord xyz;

            using IteratorType = itk::ImageRegionIteratorWithIndex<ImageType>;
            // iterator reading image value
            IteratorType imageIter(image, image->GetRequestedRegion());
            for (imageIter.GoToBegin(); !imageIter.IsAtEnd(); ++imageIter)
            {
                ImageType::IndexType index = imageIter.GetIndex(); // get itk coordinate
                PixelType value = imageIter.Get();                 // get itk value
                xyz.reset(index[0], index[1], index[2]);           // set OpenVDB coordinate
                accessor.setValue(xyz, value);                     // set OpenVDB value
            }

            // Add the grid pointer to a container.
            openvdb::GridPtrVec grids;
            grids.push_back(grid);

            // Write out the contents of the container.
            std::string outputfile = _dirName + ".vdb";
            openvdb::io::File file(outputfile);
            file.write(grids);
            file.close();

            sciter::value res;
            res.set_item("data", "ok");
            res.set_item("state", 0);
            // callback.call(res);
        }
    }
    catch (const itk::ExceptionObject &ex)
    {
        sciter::value res;
        res.set_item("data", "error");
        res.set_item("state", 1);
        callback.call(res);
        return 0;
    }

    return 0;
}

int _getMetaInfo(sciter::value dirName, sciter::value callback)
{
    std::string _dirName = dirName.get<std::string>();
    itk::GDCMImageIOFactory::RegisterOneFactory();
    using NamesGeneratorType = itk::GDCMSeriesFileNames;
    std::wstring info;
    auto nameGenerator = NamesGeneratorType::New();

    nameGenerator->SetUseSeriesDetails(true);
    nameGenerator->AddSeriesRestriction("0008|0021");
    nameGenerator->SetGlobalWarningDisplay(false);
    nameGenerator->SetDirectory(_dirName);

    try
    {
        using SeriesIdContainer = std::vector<std::string>;
        const SeriesIdContainer &seriesUID = nameGenerator->GetSeriesUIDs();
        auto seriesItr = seriesUID.begin();
        auto seriesEnd = seriesUID.end();

        if (seriesItr == seriesEnd)
        {
            sciter::value res;
            res.set_item("data", "no dicoms");
            res.set_item("state", 1);
            callback.call(res);
            return 0;
        }

        seriesItr = seriesUID.begin();
        while (seriesItr != seriesUID.end())
        {
            std::string seriesID = seriesItr->c_str();
            seriesItr++;

            using FileNamesContainer = std::vector<std::string>;
            FileNamesContainer fileNames = nameGenerator->GetFileNames(seriesID);

            using PixelType = float;
            constexpr unsigned int Dimension = 3;
            using ImageType = itk::Image<PixelType, Dimension>;
            using ReaderType = itk::ImageSeriesReader<ImageType>;
            using ImageIOType = itk::GDCMImageIO;
            using IteratorType = itk::ImageRegionIteratorWithIndex<ImageType>;
            using StatisticsImageFilterType = itk::StatisticsImageFilter<ImageType>;

            auto reader = ReaderType::New();
            auto dicomIO = ImageIOType::New();
            auto statFilter = StatisticsImageFilterType::New();

            reader->SetImageIO(dicomIO);
            reader->SetFileNames(fileNames);
            reader->ForceOrthogonalDirectionOff(); // properly read CTs with gantry tilt
            reader->Update();

            ImageType::Pointer inputImage = reader->GetOutput();
            ImageType::SpacingType inputSpacing = inputImage->GetSpacing();
            ImageType::RegionType inputRegion = inputImage->GetLargestPossibleRegion();
            ImageType::SizeType inputSize = inputRegion.GetSize();

            statFilter->SetInput(inputImage);
            statFilter->Update();
            float inputMin = statFilter->GetMinimum();
            float inputMax = statFilter->GetMaximum();
            std::vector<float> inputSize_f(inputSize.begin(), inputSize.end());
            std::vector<float> inputSpacing_f(inputSpacing.begin(), inputSpacing.end());

            sciter::value res;
            sciter::value data;

            data.set_item("input_min", inputMin);
            data.set_item("input_max", inputMax);
            data.set_item("input_spacing", inputSpacing_f);
            data.set_item("input_size", inputSize_f);
            res.set_item("data", data);
            res.set_item("state", 0);
            callback.call(res);
        }
    }
    catch (const itk::ExceptionObject &ex)
    {
        sciter::value res;
        res.set_item("data", "error");
        res.set_item("state", 1);
        callback.call(res);
        return 0;
    }
    return 0;
}

class appWindow : public sciter::window
{
public:
    appWindow() : window(SW_TITLEBAR | SW_RESIZEABLE | SW_CONTROLS | SW_MAIN | SW_ENABLE_DEBUG) {}
    // passport - lists native functions and properties exposed to script under 'appWindow' interface name:
    SOM_PASSPORT_BEGIN(appWindow)
    SOM_FUNCS(
        SOM_FUNC(convertDICOM),
        SOM_FUNC(getMetaInfo))
    SOM_PASSPORT_END
    // function expsed to script:
    int convertDICOM(sciter::value dirName, sciter::value ouputMeta, sciter::value callback)
    {
        std::thread t(_convertDICOM, dirName, ouputMeta, callback);
        t.detach();
        return 0;
    }

    int getMetaInfo(sciter::value dirName, sciter::value callback)
    {
        std::thread t(_getMetaInfo, dirName, callback);
        t.detach();
        return 0;
    }
};

#include "resources.cpp"

int uimain(std::function<int()> run)
{
    sciter::archive::instance().open(aux::elements_of(resources));
    sciter::om::hasset<appWindow> pwin = new appWindow();
    pwin->load(WSTR("this://app/main.html"));
    // or use this to load UI from
    //   pwin->load( WSTR("file:///home/andrew/Desktop/Project/res/main.htm") );
    pwin->expand();
    return run();
}