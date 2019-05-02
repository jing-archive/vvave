#include "brain.h"
#include "../services/local/taginfo.h"
#include "../utils/babeconsole.h"
#include "../db/collectionDB.h"

Brain::Brain() : QObject (nullptr)
{
    this->db = CollectionDB::getInstance();
    qRegisterMetaType<FMH::MODEL>("FMH::MODEL");
    qRegisterMetaType<TABLE>("TABLE");
    qRegisterMetaType<PULPO::RESPONSE>("PULPO::RESPONSE");
    this->pulpo = new Pulpo(this);

    connect(pulpo, &Pulpo::finished, [this]()
    {
        if(this->queue.hasNext())
        {
//             this->t.msleep(3000);
            this->pulpo->request(this->queue.next());
        }else
        {
            qDebug()<< "NOTHING ELSE IN QUEUE";
            emit this->done(TABLE::ALBUMS);
        }
    });

    this->moveToThread(&t);
    this->t.start();
}

Brain::~Brain()
{
    qDebug()<<"Deleting Brainz obj";
    this->stop();
}

void Brain::start()
{
    if(this->isRunning()) return;

    this->go = true;
    QMetaObject::invokeMethod(this, "synapse");
}

void Brain::stop()
{
    this->go = false;
    this->t.quit();
    this->t.wait();
}

void Brain::pause()
{
    this->go = false;
}

bool Brain::isRunning() const
{
    return this->go;
}

void Brain::setInterval(const uint &value)
{
    qDebug()<< "reseting the interval brainz";
    this->interval = value;
}

void Brain::appendRequest(const REQUEST &request)
{
    this->queue << request;
}


void Brain::synapse()
{
    if(this->go)
    {
        this->artworks();
        //        this->trackLyrics();
        //        this->tags();
        //        this->wikis();
    }

    if(this->queue.hasNext())
        this->pulpo->request(this->queue.next());


    this->go = false;
}

void Brain::connectionParser(PULPO::REQUEST request, PULPO::RESPONSES responses)
{
    if(this->queue.hasNext())
    {
        this->pulpo->request(this->queue.next());
    }else
    {
        qDebug()<< "NOTHING ELSE IN QUEUE";
        emit this->done(TABLE::ALBUMS);
    }

    for(auto res : responses)
        qDebug()<< PULPO::CONTEXT_MAP[res.context] << res.value;



//    for(const auto &res : responses)
//    {
//        for(const auto &onto : res.keys())
//            switch(onto)
//            {
//            case ONTOLOGY::ALBUM: this->parseAlbumInfo(request.track, res[onto]); break;
//            case ONTOLOGY::ARTIST: this->parseArtistInfo(request.track, res[onto]); break;
//            case ONTOLOGY::TRACK:  this->parseTrackInfo(request.track, response[res]); break;
//            }
//    }
}

void Brain::parseAlbumInfo(FMH::MODEL &track, const INFO_K &response)
{

    for(auto info : response.keys())
        switch(info)
        {
        case PULPO::INFO::TAGS:
        {
            for(auto context : response[info].keys())

                if(!response[info][context].toMap().isEmpty())
                {
                    for(auto tag : response[info][context].toMap().keys() )
                        this->db->tagsAlbum(track, tag, CONTEXT_MAP[context]);

                }else if (!response[info][context].toStringList().isEmpty())
                {
                    for(auto tag : response[info][context].toStringList() )
                        this->db->tagsAlbum(track, tag, CONTEXT_MAP[context]);

                } else if (!response[info][context].toString().isEmpty())
                {
                    this->db->tagsAlbum(track, response[info][context].toString(), CONTEXT_MAP[context]);
                }
            break;
        }

        case PULPO::INFO::ARTWORK:
        {
            if(!response[info].isEmpty())

                if(!response[info][CONTEXT::IMAGE].toString().isEmpty())
                {
                    qDebug()<<"SAVING ARTWORK FOR: " << track[FMH::MODEL_KEY::ALBUM];
                    auto downloader = new FMH::Downloader;



                    connect(downloader, &FMH::Downloader::fileSaved, [=](QString path)
                    {
                        qDebug()<< "Saving artwork file to" << path;
                        FMH::MODEL newTrack = track;
                        newTrack[FMH::MODEL_KEY::ARTWORK] = path;
                        this->db->insertArtwork(newTrack);

                        downloader->deleteLater();

                    });

                    QStringList filePathList = response[info][CONTEXT::IMAGE].toString().split('/');
                    const auto format = "." + filePathList.at(filePathList.count() - 1).split(".").last();
                    QString name = !track[FMH::MODEL_KEY::ALBUM].isEmpty() ? track[FMH::MODEL_KEY::ARTIST] + "_" + track[FMH::MODEL_KEY::ALBUM] : track[FMH::MODEL_KEY::ARTIST];
                    name.replace("/", "-");
                    name.replace("&", "-");
                    downloader->setFile(response[info][CONTEXT::IMAGE].toString(),  BAE::CachePath + name + format);
                }

            break;
        }

        case PULPO::INFO::WIKI:
        {
            if(!response[info].isEmpty())
                for (auto context : response[info].keys())
                    this->db->wikiAlbum(track, response[info][context].toString());
            break;
        }

        default: continue;
        }
}

void Brain::parseArtistInfo(FMH::MODEL &track, const INFO_K &response)
{
    for(auto info : response.keys())
    {
        switch(info)
        {
        case PULPO::INFO::TAGS:
        {
            if(!response[info].isEmpty())
            {
                for(auto context : response[info].keys())
                {
                    if(!response[info][context].toMap().isEmpty())
                    {
                        for(auto tag : response[info][context].toMap().keys() )
                            this->db->tagsArtist(track, tag, CONTEXT_MAP[context]);

                    }else if(!response[info][context].toStringList().isEmpty())
                    {
                        for(auto tag : response[info][context].toStringList() )
                            this->db->tagsArtist(track, tag, CONTEXT_MAP[context]);

                    }else if(!response[info][context].toString().isEmpty())
                    {
                        this->db->tagsArtist(track, response[info][context].toString(), CONTEXT_MAP[context]);
                    }
                }

            } break;
        }

        case PULPO::INFO::ARTWORK:
        {
            if(!response[info].isEmpty())
            {
                if(!response[info][CONTEXT::IMAGE].toByteArray().isEmpty())
                {
                    BAE::saveArt(track, response[info][CONTEXT::IMAGE].toByteArray(), BAE::CachePath);
                    this->db->insertArtwork(track);
                }
            }

            break;
        }

        case PULPO::INFO::WIKI:
        {
            if(!response[info].isEmpty())
            {
                for (auto context : response[info].keys())
                    this->db->wikiArtist(track, response[info][context].toString());
            }

            break;
        }

        default: continue;
        }
    }
}

void Brain::parseTrackInfo(FMH::MODEL &track, const INFO_K &response)
{
    for(auto info : response.keys())
        switch(info)
        {
        case PULPO::INFO::TAGS:
        {
            if(!response[info].isEmpty())
            {
                for(auto context : response[info].keys())
                {
                    if (!response[info][context].toStringList().isEmpty())
                    {
                        for(auto tag : response[info][context].toStringList() )
                            this->db->tagsTrack(track, tag, CONTEXT_MAP[context]);
                    }

                    if (!response[info][context].toString().isEmpty())
                        this->db->tagsTrack(track, response[info][context].toString(), CONTEXT_MAP[context]);
                }
            }

            break;
        }

        case PULPO::INFO::WIKI:
        {
            if(!response[info].isEmpty())
            {
                if (!response[info][CONTEXT::WIKI].toString().isEmpty())
                    this->db->wikiTrack(track, response[info][CONTEXT::WIKI].toString());

            }

            break;
        }

        case PULPO::INFO::ARTWORK:
        {
            if(!response[info].isEmpty())
            {
                if(!response[info][CONTEXT::IMAGE].toByteArray().isEmpty())
                {
                    BAE::saveArt(track, response[info][CONTEXT::IMAGE].toByteArray(),CachePath);
                    this->db->insertArtwork(track);
                }
            }

            break;
        }

        case PULPO::INFO::METADATA:
        {
            TagInfo tag;
            for(auto context :response[info].keys())
            {
                switch(context)
                {
                case CONTEXT::ALBUM_TITLE:
                {
                    qDebug()<<"SETTING TRACK MISSING METADATA";

                    tag.feed(track[FMH::MODEL_KEY::URL]);
                    if(!response[info][context].toString().isEmpty())
                    {
                        tag.setAlbum(response[info][context].toString());
                        this->db->albumTrack(track, response[info][context].toString());
                    }

                    break;
                }

                case CONTEXT::TRACK_NUMBER:
                {
                    tag.feed(track[FMH::MODEL_KEY::URL]);
                    if(!response[info][context].toString().isEmpty())
                        tag.setTrack(response[info][context].toInt());

                    break;
                }

                default: continue;
                }
            }

            break;
        }

        case PULPO::INFO::LYRICS:
        {
            if(!response[info][CONTEXT::LYRIC].toString().isEmpty())
                this->db->lyricsTrack(track, response[info][CONTEXT::LYRIC].toString());
            break;
        }

        default: continue;
        }
}

void Brain::trackInfo()
{
    //        auto queryTxt =  QString("SELECT %1, %2, %3 FROM %4 WHERE %3 = 'UNKNOWN' GROUP BY %2, a.%3 ").arg(KEYMAP[KEY::TITLE],
    //                KEYMAP[KEY::ARTIST],KEYMAP[KEY::ALBUM],TABLEMAP[TABLE::TRACKS]);
    //        QSqlQuery query (queryTxt);
    //        pulpo.setInfo(INFO::METADATA);
    //        for(auto track : this->getDBData(query))
    //        {
    //            qDebug()<<"UNKOWN TRACK TITLE:"<<track[KEY::TITLE];
    //            pulpo.feed(track,RECURSIVE::OFF);
    //            if(!go) return;
    //        }

    // emit this->done(TABLE::TRACKS);

    this->trackArtworks();
    this->trackLyrics();
    this->trackTags();
    this->trackWikis();
}

void Brain::albumInfo()
{
    if(!go) return;

    this->albumArtworks();
    this->albumTags();
    this->albumWikis();
}

void Brain::artistInfo()
{
    if(!go) return;

    this->artistArtworks();
    this->artistTags();
    this->artistWikis();
}

void Brain::artworks()
{
    if(!go) return;

    this->albumArtworks();
    //    this->artistArtworks();
    //    this->trackArtworks();
}

void Brain::tags()
{
    if(!go) return;

    this->albumTags();
    this->artistTags();
    this->trackTags();
}

void Brain::wikis()
{
    if(!go) return;

    this->albumWikis();
    this->artistWikis();
    this->trackWikis();
}

void Brain::albumArtworks()
{
    if(!this->go) return;

    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz};
    auto ontology = PULPO::ONTOLOGY::ALBUM;

    auto queryTxt = QString("SELECT %1, %2 FROM %3 WHERE %4 = ''").arg(KEYMAP[KEY::ALBUM],
            KEYMAP[KEY::ARTIST], TABLEMAP[TABLE::ALBUMS], KEYMAP[KEY::ARTWORK]);

    /* BEFORE FETCHING ONLINE LOOK UP IN THE CACHE FOR THE IMAGES*/
    auto artworks = this->db->getDBData(queryTxt);
    for(auto album : artworks)
        if(BAE::artworkCache(album, FMH::MODEL_KEY::ALBUM))
            this->db->insertArtwork(album);

    artworks = this->db->getDBData(queryTxt);
    qDebug() << "Getting missing albums artworks"<< artworks.length();

    FMH::MODEL_LIST data = {
        {{FMH::MODEL_KEY::ARTIST, "Kali Uchis"}, {FMH::MODEL_KEY::ALBUM, "Por Vida"}},
        {{FMH::MODEL_KEY::ARTIST, "Laura Marling"}, {FMH::MODEL_KEY::ALBUM, "Once I Was An Eagle"}},
        {{FMH::MODEL_KEY::ARTIST, "Lorde"}, {FMH::MODEL_KEY::ALBUM, "Melodrama"}},
        {{FMH::MODEL_KEY::ARTIST, "Rihanna"}, {FMH::MODEL_KEY::ALBUM, "ANTI"}},
//        {{FMH::MODEL_KEY::ARTIST, "Chris Isaak"}, {FMH::MODEL_KEY::ALBUM, "Heart Shaped World"}},
//        {{FMH::MODEL_KEY::ARTIST, "Choker"}, {FMH::MODEL_KEY::ALBUM, "Peak"}},
//        {{FMH::MODEL_KEY::ARTIST, "John Mayer"}, {FMH::MODEL_KEY::ALBUM, "Continuum"}},
//        {{FMH::MODEL_KEY::ARTIST, "070 Shake"}, {FMH::MODEL_KEY::ALBUM, "Glitter"}},
//        {{FMH::MODEL_KEY::ARTIST, "Snoh Aalegra"}, {FMH::MODEL_KEY::ALBUM, "FEELS"}},
//        {{FMH::MODEL_KEY::ARTIST, "Sonder"}, {FMH::MODEL_KEY::ALBUM, "Into"}},
//        {{FMH::MODEL_KEY::ARTIST, "JONES"}, {FMH::MODEL_KEY::ALBUM, "New Skin"}},
//        {{FMH::MODEL_KEY::ARTIST, "Artic Monkeys"}, {FMH::MODEL_KEY::ALBUM, "AM"}},
//        {{FMH::MODEL_KEY::ARTIST, "Lady Gaga"}, {FMH::MODEL_KEY::ALBUM, "ARTPOP"}},
//        {{FMH::MODEL_KEY::ARTIST, "Aimee Mann"}, {FMH::MODEL_KEY::ALBUM, "I'm With Stupid"}},
//        {{FMH::MODEL_KEY::ARTIST, "Smith Westerns"}, {FMH::MODEL_KEY::ALBUM, "Dye It Blonde"}},
//        {{FMH::MODEL_KEY::ARTIST, "J. Cole"}, {FMH::MODEL_KEY::ALBUM, "2014 Forest Hills Drive"}},
//        {{FMH::MODEL_KEY::ARTIST, "Frank Ocean"}, {FMH::MODEL_KEY::ALBUM, "Channel Orange"}},
//        {{FMH::MODEL_KEY::ARTIST, "The Weeknd"}, {FMH::MODEL_KEY::ALBUM, "Trilogy"}},
//        {{FMH::MODEL_KEY::ARTIST, "Lana Del Rey"}, {FMH::MODEL_KEY::ALBUM, "Honeymoon"}},
//        {{FMH::MODEL_KEY::ARTIST, "Lana Del Rey"}, {FMH::MODEL_KEY::ALBUM, "Ultraviolence"}}
    };


    for(const auto &item : artworks)
    {
        REQUEST request;
        request.track = item;
        request.ontology =  ontology;
        request.services = services;
        request.info = {PULPO::INFO::ARTWORK};
        request.callback = [this](PULPO::REQUEST request, PULPO::RESPONSES responses)
        {
            qDebug() << "DONE WITH " << request.track ;

            for(const auto &res : responses)
            {
                if(res.context == PULPO::CONTEXT::IMAGE && !res.value.toString().isEmpty())
                {
                    qDebug()<<"SAVING ARTWORK FOR: " << request.track[FMH::MODEL_KEY::ALBUM];
                    auto downloader = new FMH::Downloader;
                    connect(downloader, &FMH::Downloader::fileSaved, [=](QString path)
                    {
                        qDebug()<< "Saving artwork file to" << path;
                        FMH::MODEL newTrack = request.track;
                        newTrack[FMH::MODEL_KEY::ARTWORK] = path;
                        this->db->insertArtwork(newTrack);

                        downloader->deleteLater();

                    });

                    QStringList filePathList = res.value.toString().split('/');
                    const auto format = "." + filePathList.at(filePathList.count() - 1).split(".").last();
                    QString name = !request.track[FMH::MODEL_KEY::ALBUM].isEmpty() ? request.track[FMH::MODEL_KEY::ARTIST] + "_" + request.track[FMH::MODEL_KEY::ALBUM] : request.track[FMH::MODEL_KEY::ARTIST];
                    name.replace("/", "-");
                    name.replace("&", "-");
                    downloader->setFile(res.value.toString(),  BAE::CachePath + name + format);

                }
            }
        };

//        this->queue << request;
    }
    //    emit this->done(TABLE::ALBUMS);
}

void Brain::albumTags()
{
    if(!this->go) return;

    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz};
    auto ontology = PULPO::ONTOLOGY::ALBUM;

    //select album, artist from albums where  album  not in (select album from albums_tags) and artist  not in (select  artist from albums_tags)
    qDebug() << ("Getting missing albums tags");
    auto queryTxt =  QString("SELECT %1, %2 FROM %3 WHERE %1 NOT IN ( SELECT %1 FROM %4 ) AND %2 NOT IN ( SELECT %2 FROM %4 )").arg(KEYMAP[KEY::ALBUM],
            KEYMAP[KEY::ARTIST],
            TABLEMAP[TABLE::ALBUMS],
            TABLEMAP[TABLE::ALBUMS_TAGS]);

    //    REQUEST request;
    //    request.data = this->db->getDBData(queryTxt);
    //    request.ontology =  ontology;
    //    request.services = services;
    //    request.info = PULPO::INFO::TAGS;
    //    request.recursive = PULPO::RECURSIVE::ON;
    //    request.cb = nullptr;

    //    this->appendRequest(request);
}

void Brain::albumWikis()
{
    if(!this->go) return;

    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz};
    auto ontology = PULPO::ONTOLOGY::ALBUM;

    qDebug() << ("Getting missing albums wikis");
    auto queryTxt =  QString("SELECT %1, %2 FROM %3 WHERE %4 = '' ").arg(KEYMAP[KEY::ALBUM],
            KEYMAP[KEY::ARTIST],
            TABLEMAP[TABLE::ALBUMS],
            KEYMAP[KEY::WIKI]);


    //    this->setInfo(this->db->getDBData(queryTxt), ontology, services, PULPO::INFO::WIKI, PULPO::RECURSIVE::OFF, nullptr);
}

void Brain::artistArtworks()
{
    if(!this->go) return;

    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz, PULPO::SERVICES::Genius};
    auto ontology = PULPO::ONTOLOGY::ARTIST;

    qDebug() << ("Getting missing artists artworks");
    auto queryTxt = QString("SELECT %1 FROM %2 WHERE %3 = ''").arg(KEYMAP[KEY::ARTIST],
            TABLEMAP[TABLE::ARTISTS],
            KEYMAP[KEY::ARTWORK]);
    auto artworks = this->db->getDBData(queryTxt);


    /* BEFORE FETCHING ONLINE LOOK UP IN THE CACHE FOR THE IMAGE */
    //    for(auto artist : artworks)
    //        if(BAE::artworkCache(artist, KEY::ARTIST))
    //            this->insertArtwork(artist);

    artworks = this->db->getDBData(queryTxt);
    //    this->setInfo(artworks, ontology, services, PULPO::INFO::ARTWORK, PULPO::RECURSIVE::OFF, nullptr);

    emit this->done(TABLE::ARTISTS);
}

void Brain::artistTags()
{
    if(!this->go) return;

    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz, PULPO::SERVICES::Genius};
    auto ontology = PULPO::ONTOLOGY::ARTIST;

    //select artist from artists where  artist  not in (select album from albums_tags)
    qDebug() << ("Getting missing artists tags");
    auto queryTxt =  QString("SELECT %1 FROM %2 WHERE %1 NOT IN ( SELECT %1 FROM %3 ) ").arg(KEYMAP[KEY::ARTIST],
            TABLEMAP[TABLE::ARTISTS],
            TABLEMAP[TABLE::ARTISTS_TAGS]);
    //    this->setInfo(this->db->getDBData(queryTxt), ontology, services, PULPO::INFO::TAGS, PULPO::RECURSIVE::ON, nullptr);

}

void Brain::artistWikis()
{
    if(!this->go) return;

    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz, PULPO::SERVICES::Genius};
    auto ontology = PULPO::ONTOLOGY::ARTIST;

    qDebug() << ("Getting missing artists wikis");
    auto queryTxt =  QString("SELECT %1 FROM %2 WHERE %3 = '' ").arg(KEYMAP[KEY::ARTIST],
            TABLEMAP[TABLE::ARTISTS],
            KEYMAP[KEY::WIKI]);
    //    this->setInfo(this->db->getDBData(queryTxt), ontology, services, PULPO::INFO::WIKI, PULPO::RECURSIVE::OFF, nullptr);
}

void Brain::trackArtworks()
{
    if(!this->go) return;

    auto ontology = PULPO::ONTOLOGY::TRACK;
    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz, PULPO::SERVICES::Genius};

    qDebug() << ("Getting missing track artwork");
    //select url, title, album, artist from tracks t inner join albums a on a.album=t.album and a.artist=t.artist where a.artwork = ''
    auto queryTxt =  QString("SELECT DISTINCT t.%1, t.%2, t.%3, t.%4 FROM %5 t INNER JOIN %6 a ON a.%3 = t.%3 AND a.%4 = t.%4  WHERE a.%7 = '' GROUP BY a.%3, a.%4 ").arg(KEYMAP[KEY::URL],
            KEYMAP[KEY::TITLE],
            KEYMAP[KEY::ARTIST],
            KEYMAP[KEY::ALBUM],
            TABLEMAP[TABLE::TRACKS],
            TABLEMAP[TABLE::ALBUMS],
            KEYMAP[KEY::ARTWORK]);

    auto artworks = this->db->getDBData(queryTxt);
    //    this->setInfo(artworks, ontology, services, PULPO::INFO::ARTWORK, PULPO::RECURSIVE::OFF);

    emit this->done(TABLE::ALBUMS);

}

void Brain::trackLyrics()
{
    if(!this->go) return;

    auto ontology = PULPO::ONTOLOGY::TRACK;
    auto services = {PULPO::SERVICES::LyricWikia, PULPO::SERVICES::Genius};

    qDebug() << ("Getting missing track lyrics");
    auto queryTxt = QString("SELECT %1, %2, %3 FROM %4 WHERE %5 = ''").arg(KEYMAP[KEY::URL],
            KEYMAP[KEY::TITLE],
            KEYMAP[KEY::ARTIST],
            TABLEMAP[TABLE::TRACKS],
            KEYMAP[KEY::LYRICS]);

    //    this->setInfo(this->db->getDBData(queryTxt), ontology, services, PULPO::INFO::LYRICS, PULPO::RECURSIVE::OFF);

}

void Brain::trackTags()
{
    if(!this->go) return;

    auto ontology = PULPO::ONTOLOGY::TRACK;
    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz, PULPO::SERVICES::Genius};

    qDebug() << ("Getting missing track tags");
    // select title, artist, album from tracks t where url not in (select url from tracks_tags)
    auto queryTxt = QString("SELECT %1, %2, %3, %4 FROM %5 WHERE %1 NOT IN ( SELECT %1 FROM %6 )").arg(KEYMAP[KEY::URL],
            KEYMAP[KEY::TITLE],
            KEYMAP[KEY::ARTIST],
            KEYMAP[KEY::ALBUM],
            TABLEMAP[TABLE::TRACKS],
            TABLEMAP[TABLE::TRACKS_TAGS]);
    //    this->setInfo(this->db->getDBData(queryTxt), ontology, services, PULPO::INFO::TAGS, RECURSIVE::ON, nullptr);
}

void Brain::trackWikis()
{
    if(!this->go) return;

    auto ontology = PULPO::ONTOLOGY::TRACK;
    auto services = {PULPO::SERVICES::LastFm, PULPO::SERVICES::Spotify, PULPO::SERVICES::MusicBrainz, PULPO::SERVICES::Genius};

    qDebug() << ("Getting missing track wikis");
    auto queryTxt = QString("SELECT %1, %2, %3, %4 FROM %5 WHERE %6 = ''").arg(KEYMAP[KEY::URL],
            KEYMAP[KEY::TITLE],
            KEYMAP[KEY::ARTIST],
            KEYMAP[KEY::ALBUM],
            TABLEMAP[TABLE::TRACKS],
            KEYMAP[KEY::WIKI]);
    //    this->setInfo(this->db->getDBData(queryTxt), ontology, services, PULPO::INFO::WIKI, RECURSIVE::OFF);

}
