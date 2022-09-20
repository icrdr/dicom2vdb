#include "sciter-x.h"
#include "sciter-x-window.hpp"
#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"
#include "itkGDCMImageIOFactory.h"
#include "itkNrrdImageIOFactory.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkStatisticsImageFilter.h"
#include "openvdb/openvdb.h"

int convertDICOM2VDB(std::string dirName)
{
    itk::GDCMImageIOFactory::RegisterOneFactory();
    using NamesGeneratorType = itk::GDCMSeriesFileNames;
    auto nameGenerator = NamesGeneratorType::New();

    nameGenerator->SetUseSeriesDetails(true);
    nameGenerator->AddSeriesRestriction("0008|0021");
    nameGenerator->SetGlobalWarningDisplay(false);
    nameGenerator->SetDirectory(dirName);

    try
    {
        using SeriesIdContainer = std::vector<std::string>;
        const SeriesIdContainer &seriesUID = nameGenerator->GetSeriesUIDs();
        auto seriesItr = seriesUID.begin();
        auto seriesEnd = seriesUID.end();

        if (seriesItr != seriesEnd)
        {
            std::cout << "The directory: ";
            std::cout << dirName << std::endl;
            std::cout << "Contains the following DICOM Series: ";
            std::cout << std::endl;
        }
        else
        {
            std::cout << "No DICOMs in: " << dirName << std::endl;
            return EXIT_SUCCESS;
        }

        while (seriesItr != seriesEnd)
        {
            std::cout << seriesItr->c_str() << std::endl;
            ++seriesItr;
        }

        seriesItr = seriesUID.begin();
        while (seriesItr != seriesUID.end())
        {
            std::string seriesIdentifier;

            seriesIdentifier = seriesItr->c_str();
            seriesItr++;

            std::cout << "\nReading: ";
            std::cout << seriesIdentifier << std::endl;
            using FileNamesContainer = std::vector<std::string>;
            FileNamesContainer fileNames = nameGenerator->GetFileNames(seriesIdentifier);

            using PixelType = float;
            constexpr unsigned int Dimension = 3;
            using ImageType = itk::Image<PixelType, Dimension>;
            using ReaderType = itk::ImageSeriesReader<ImageType>;
            using ImageIOType = itk::GDCMImageIO;
            using IteratorType = itk::ImageRegionIteratorWithIndex<ImageType>;
            using StatisticsImageFilterType = itk::StatisticsImageFilter<ImageType>;

            auto reader = ReaderType::New();
            auto dicomIO = ImageIOType::New();
            auto statisticsImageFilter = StatisticsImageFilterType::New();

            reader->SetImageIO(dicomIO);
            reader->SetFileNames(fileNames);
            reader->ForceOrthogonalDirectionOff(); // properly read CTs with gantry tilt
            reader->Update();

            ImageType::Pointer image = reader->GetOutput();
            ImageType::SpacingType inputSpacing = image->GetSpacing();
            ImageType::RegionType inputRegion = image->GetLargestPossibleRegion();
            ImageType::SizeType inputSize = inputRegion.GetSize();

            statisticsImageFilter->SetInput(image);
            statisticsImageFilter->Update();
            double min = statisticsImageFilter->GetMinimum();
            double max = statisticsImageFilter->GetMaximum();

            std::cout << "Min: " << min << std::endl;
            std::cout << "Max: " << max << std::endl;
            std::cout << "Spacing: " << inputSpacing << std::endl;
            std::cout << "Size: " << inputSize << std::endl;

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

            std::cout << "Writing VDB file..." << std::endl;
            // iterator reading image value
            IteratorType imageIter(image, image->GetRequestedRegion());
            for (imageIter.GoToBegin(); !imageIter.IsAtEnd(); ++imageIter)
            {
                ImageType::IndexType index = imageIter.GetIndex(); // get itk coordinate
                PixelType value = imageIter.Get();                 // get itk value
                xyz.reset(index[0], index[1], index[2]);           // set OpenVDB coordinate
                accessor.setValue(xyz, value);                     // set OpenVDB value
                // std::cout << "Grid" << xyz << " = " << accessor.getValue(xyz) << std::endl;
            }

            // Add the grid pointer to a container.
            openvdb::GridPtrVec grids;
            grids.push_back(grid);

            // Write out the contents of the container.
            std::string outputfile = dirName + ".vdb";
            openvdb::io::File file(outputfile);
            file.write(grids);
            file.close();
            std::cout << "Completed!" << std::endl;
        }
    }
    catch (const itk::ExceptionObject &ex)
    {
        std::cout << ex << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

class frame : public sciter::window
{
public:
    frame() : window(SW_TITLEBAR | SW_RESIZEABLE | SW_CONTROLS | SW_MAIN | SW_ENABLE_DEBUG) {}
    // passport - lists native functions and properties exposed to script under 'frame' interface name:
    SOM_PASSPORT_BEGIN(frame)
    SOM_FUNCS(
        SOM_FUNC(convertDICOM))
    SOM_PASSPORT_END
    // function expsed to script:
    sciter::value convertDICOM(sciter::value param1)
    {
        std::string dirName = param1.get<std::string>();
        std::cout << dirName << std::endl;
        convertDICOM2VDB(dirName);
        return dirName;
    }
};

#include "resources.cpp"

int uimain(std::function<int()> run)
{
    sciter::archive::instance().open(aux::elements_of(resources));
    sciter::om::hasset<frame> pwin = new frame();
    pwin->load(WSTR("this://app/main.html"));
    // or use this to load UI from
    //   pwin->load( WSTR("file:///home/andrew/Desktop/Project/res/main.htm") );
    pwin->expand();
    return run();
}